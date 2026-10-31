#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <wcore/assert.hpp>
#include <wcore/types.hpp>

namespace wcore::binary
{
	using Byte = std::byte;
	using Bytes = std::vector<Byte>;
	using ByteView = std::span<const Byte>;

	[[nodiscard]] constexpr bool is_little_endian() noexcept
	{
		return std::endian::native == std::endian::little;
	}

	template <typename T>
	[[nodiscard]] T byte_swap(T value) noexcept
	{
		static_assert(std::is_trivially_copyable_v<T>,
			"byte_swap requires a trivially copyable type");
		static_assert(sizeof(T) > 1,
			"byte_swap requires a type larger than one byte");

		std::array<Byte, sizeof(T)> bytes{};
		std::memcpy(bytes.data(), &value, sizeof(T));

		for (std::size_t i = 0; i < bytes.size() / 2; ++i)
			std::swap(bytes[i], bytes[bytes.size() - 1 - i]);

		std::memcpy(&value, bytes.data(), sizeof(T));
		return value;
	}

	template <typename T>
	[[nodiscard]] T to_little_endian(T value) noexcept
	{
		if constexpr (sizeof(T) <= 1)
			return value;
		else if constexpr (std::endian::native == std::endian::little)
			return value;
		else
			return byte_swap(value);
	}

	template <typename T>
	[[nodiscard]] T from_little_endian(T value) noexcept
	{
		return to_little_endian(value);
	}

	template <typename T>
	[[nodiscard]] T to_big_endian(T value) noexcept
	{
		if constexpr (sizeof(T) <= 1)
			return value;
		else if constexpr (std::endian::native == std::endian::big)
			return value;
		else
			return byte_swap(value);
	}

	template <typename T>
	[[nodiscard]] T from_big_endian(T value) noexcept
	{
		return to_big_endian(value);
	}

	[[nodiscard]] constexpr u64 zigzag_encode(i64 value) noexcept
	{
		return (static_cast<u64>(value) << 1) ^
			static_cast<u64>(value >> 63);
	}

	[[nodiscard]] constexpr i64 zigzag_decode(u64 value) noexcept
	{
		return static_cast<i64>((value >> 1) ^ (~(value & 1) + 1));
	}

	class Writer
	{
	public:
		Writer() = default;
		explicit Writer(std::size_t reserve) { mBytes.reserve(reserve); }

		// Writes a scalar in little-endian order (enums use their underlying
		// type, and bool is written as exactly one byte).
		template <typename T>
			requires (std::is_arithmetic_v<T> || std::is_enum_v<T>)
		void write(T value)
		{
			if constexpr (std::is_enum_v<T>)
				write(static_cast<std::underlying_type_t<T>>(value));
			else if constexpr (std::is_same_v<T, bool>)
				write_pod(static_cast<u8>(value ? 1 : 0));
			else
				write_pod(to_little_endian(value));
		}

		// Writes a scalar in big-endian order.
		template <typename T>
			requires (std::is_arithmetic_v<T> || std::is_enum_v<T>)
		void write_big(T value)
		{
			if constexpr (std::is_enum_v<T>)
				write_big(static_cast<std::underlying_type_t<T>>(value));
			else if constexpr (std::is_same_v<T, bool>)
				write_pod(static_cast<u8>(value ? 1 : 0));
			else
				write_pod(to_big_endian(value));
		}

		// Writes the raw object representation, without swapping bytes.
		template <typename T>
		void write_pod(const T& value)
		{
			static_assert(std::is_trivially_copyable_v<T>,
				"write_pod requires a trivially copyable type");

			const auto* first = reinterpret_cast<const Byte*>(&value);
			mBytes.insert(mBytes.end(), first, first + sizeof(T));
		}

		void write_bytes(ByteView bytes)
		{
			mBytes.insert(mBytes.end(), bytes.begin(), bytes.end());
		}

		// Length-prefixed with a little-endian u32.
		void write_string(std::string_view text)
		{
			WCORE_ASSERT(
				text.size() <= std::numeric_limits<u32>::max(),
				"binary::Writer string is too long");

			write(static_cast<u32>(text.size()));
			if (!text.empty())
			{
				const auto* first =
					reinterpret_cast<const Byte*>(text.data());
				mBytes.insert(mBytes.end(), first, first + text.size());
			}
		}

		void write_varuint(u64 value)
		{
			while (value >= 0x80)
			{
				mBytes.push_back(
					static_cast<Byte>((value & 0x7F) | 0x80));
				value >>= 7;
			}

			mBytes.push_back(static_cast<Byte>(value));
		}

		void write_varint(i64 value)
		{
			write_varuint(zigzag_encode(value));
		}

		[[nodiscard]] const Bytes& data() const noexcept { return mBytes; }
		[[nodiscard]] Bytes& data() noexcept { return mBytes; }
		[[nodiscard]] ByteView view() const noexcept { return mBytes; }
		[[nodiscard]] std::size_t size() const noexcept
		{
			return mBytes.size();
		}
		[[nodiscard]] bool empty() const noexcept { return mBytes.empty(); }

		void reserve(std::size_t capacity) { mBytes.reserve(capacity); }
		void clear() noexcept { mBytes.clear(); }
		[[nodiscard]] Bytes release() noexcept { return std::move(mBytes); }

	private:
		Bytes mBytes;
	};

	class Reader
	{
	public:
		Reader() = default;
		explicit Reader(ByteView bytes) noexcept : mBytes(bytes) {}

		void reset(ByteView bytes) noexcept
		{
			mBytes = bytes;
			mPos = 0;
			mFailed = false;
		}

		[[nodiscard]] bool failed() const noexcept { return mFailed; }
		void clear_error() noexcept { mFailed = false; }

		[[nodiscard]] std::size_t position() const noexcept { return mPos; }
		[[nodiscard]] std::size_t remaining() const noexcept
		{
			return mBytes.size() - mPos;
		}
		[[nodiscard]] bool empty() const noexcept { return remaining() == 0; }

		// The bytes that have not been consumed yet.
		[[nodiscard]] ByteView view() const noexcept
		{
			return mBytes.subspan(mPos);
		}

		// Reads a scalar in little-endian order.
		template <typename T>
			requires (std::is_arithmetic_v<T> || std::is_enum_v<T>)
		[[nodiscard]] std::optional<T> read()
		{
			if constexpr (std::is_enum_v<T>)
			{
				using U = std::underlying_type_t<T>;

				const auto raw = read<U>();
				if (!raw) return std::nullopt;
				return static_cast<T>(*raw);
			}
			else if constexpr (std::is_same_v<T, bool>)
			{
				const auto raw = read<u8>();
				if (!raw) return std::nullopt;
				return *raw != 0;
			}
			else
			{
				T value{};
				if (!read_pod(value)) return std::nullopt;
				return from_little_endian(value);
			}
		}

		// Reads a scalar in big-endian order.
		template <typename T>
			requires (std::is_arithmetic_v<T> || std::is_enum_v<T>)
		[[nodiscard]] std::optional<T> read_big()
		{
			if constexpr (std::is_enum_v<T>)
			{
				using U = std::underlying_type_t<T>;

				const auto raw = read_big<U>();
				if (!raw) return std::nullopt;
				return static_cast<T>(*raw);
			}
			else if constexpr (std::is_same_v<T, bool>)
			{
				const auto raw = read_big<u8>();
				if (!raw) return std::nullopt;
				return *raw != 0;
			}
			else
			{
				T value{};
				if (!read_pod(value)) return std::nullopt;
				return from_big_endian(value);
			}
		}

		// Reads the raw object representation, without swapping bytes.
		template <typename T>
		bool read_pod(T& value)
		{
			static_assert(std::is_trivially_copyable_v<T>,
				"read_pod requires a trivially copyable type");

			if (!ensure(sizeof(T))) return false;

			std::memcpy(&value, mBytes.data() + mPos, sizeof(T));
			mPos += sizeof(T);
			return true;
		}

		[[nodiscard]] std::optional<ByteView> read_bytes(std::size_t count)
		{
			if (!ensure(count)) return std::nullopt;

			const ByteView result = mBytes.subspan(mPos, count);
			mPos += count;
			return result;
		}

		// Length-prefixed with a little-endian u32.
		[[nodiscard]] std::optional<std::string> read_string()
		{
			const auto length = read<u32>();
			if (!length) return std::nullopt;

			if (*length > remaining())
			{
				mFailed = true;
				return std::nullopt;
			}

			std::string result;
			if (*length != 0)
			{
				result.assign(
					reinterpret_cast<const char*>(mBytes.data() + mPos),
					*length);
			}

			mPos += *length;
			return result;
		}

		[[nodiscard]] std::optional<u64> read_varuint()
		{
			u64 result = 0;
			unsigned shift = 0;

			for (unsigned i = 0; i < 10; ++i)
			{
				const auto byte = read<u8>();
				if (!byte) return std::nullopt;

				const u64 payload = static_cast<u64>(*byte & 0x7F);

				// The tenth byte may only carry the final bit.
				if (i == 9 && payload > 1)
				{
					mFailed = true;
					return std::nullopt;
				}

				result |= payload << shift;

				if ((*byte & 0x80) == 0)
					return result;

				shift += 7;
			}

			mFailed = true;
			return std::nullopt;
		}

		[[nodiscard]] std::optional<i64> read_varint()
		{
			const auto raw = read_varuint();
			if (!raw) return std::nullopt;

			return zigzag_decode(*raw);
		}

		bool skip(std::size_t count)
		{
			if (!ensure(count)) return false;

			mPos += count;
			return true;
		}

	private:
		bool ensure(std::size_t count) noexcept
		{
			if (mFailed || count > remaining())
			{
				mFailed = true;
				return false;
			}

			return true;
		}

		ByteView mBytes;
		std::size_t mPos{ 0 };
		bool mFailed{ false };
	};
} // namespace wcore::binary
