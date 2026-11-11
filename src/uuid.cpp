#include <wcore/uuid.hpp>

#include <atomic>
#include <random>
#include <string_view>

namespace wcore
{
	namespace
	{
		constexpr int hex_value(char c) noexcept
		{
			if (c >= '0' && c <= '9') return c - '0';
			if (c >= 'a' && c <= 'f') return c - 'a' + 10;
			if (c >= 'A' && c <= 'F') return c - 'A' + 10;
			return -1;
		}

		constexpr bool is_hyphen_position(std::size_t index) noexcept
		{
			return index == 8 || index == 13 || index == 18 || index == 23;
		}

		std::mt19937_64& rng()
		{
			static std::atomic<u64> counter{ 0 };

			thread_local std::mt19937_64 engine{ [counter = counter.fetch_add(
				1, std::memory_order_relaxed)]
			{
				std::random_device device;
				std::seed_seq seed{
					device(), device(), device(), device(),
					static_cast<u32>(counter), static_cast<u32>(counter >> 32) };
				return std::mt19937_64{ seed };
			}() };

			return engine;
		}
	} // anonymous namespace

	Uuid Uuid::generate()
	{
		std::uniform_int_distribution<u64> distribution;

		const u64 high{ distribution(rng()) };
		const u64 low{ distribution(rng()) };

		std::array<u8, ByteCount> bytes{};
		for (std::size_t i{ 0 }; i < 8; ++i)
		{
			bytes[i] = static_cast<u8>(high >> (56 - i * 8));
			bytes[8 + i] = static_cast<u8>(low >> (56 - i * 8));
		}

		// Version 4 and RFC 4122 variant bits.
		bytes[6] = static_cast<u8>((bytes[6] & 0x0F) | 0x40);
		bytes[8] = static_cast<u8>((bytes[8] & 0x3F) | 0x80);

		return Uuid{ bytes };
	}

	std::optional<Uuid> Uuid::from_string(std::string_view text) noexcept
	{
		std::array<char, CompactStringLength> digits{};
		std::size_t count{ 0 };

		if (text.size() == StringLength)
		{
			digits.fill('\0');

			for (std::size_t i{ 0 }; i < text.size(); ++i)
			{
				if (is_hyphen_position(i))
				{
					if (text[i] != '-') return std::nullopt;
					continue;
				}

				digits[count++] = text[i];
			}
		}
		else if (text.size() == CompactStringLength)
		{
			for (std::size_t i{ 0 }; i < text.size(); ++i)
				digits[i] = text[i];
			count = text.size();
		}
		else
		{
			return std::nullopt;
		}

		if (count != CompactStringLength) return std::nullopt;

		std::array<u8, ByteCount> bytes{};
		for (std::size_t i{ 0 }; i < ByteCount; ++i)
		{
			const int high{ hex_value(digits[i * 2]) };
			const int low{ hex_value(digits[i * 2 + 1]) };
			if (high < 0 || low < 0) return std::nullopt;

			bytes[i] = static_cast<u8>((high << 4) | low);
		}

		return Uuid{ bytes };
	}

	std::optional<Uuid> Uuid::from_bytes(
		std::span<const u8, ByteCount> bytes) noexcept
	{
		std::array<u8, ByteCount> copy{};
		for (std::size_t i{ 0 }; i < ByteCount; ++i)
			copy[i] = bytes[i];

		return Uuid{ copy };
	}

	std::string Uuid::to_string() const
	{
		static constexpr char HexDigits[]{ "0123456789abcdef" };

		std::string result(StringLength, '-');

		std::size_t out{ 0 };
		for (std::size_t i{ 0 }; i < ByteCount; ++i)
		{
			if (is_hyphen_position(out)) ++out;

			result[out++] = HexDigits[mBytes[i] >> 4];
			result[out++] = HexDigits[mBytes[i] & 0x0F];
		}

		return result;
	}
}
