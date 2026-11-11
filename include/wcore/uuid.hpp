#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <format>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

#include <wcore/types.hpp>

namespace wcore
{
	class Uuid
	{
	public:
		static constexpr std::size_t ByteCount{ 16 };
		static constexpr std::size_t StringLength{ 36 };
		static constexpr std::size_t CompactStringLength{ 32 };

		constexpr Uuid() = default;
		explicit constexpr Uuid(std::array<u8, ByteCount> bytes) noexcept
			: mBytes(bytes) {}

		static constexpr Uuid nil() noexcept { return {}; }

		[[nodiscard]] static Uuid generate();

		[[nodiscard]] static std::optional<Uuid> from_string(
			std::string_view text) noexcept;

		[[nodiscard]] static std::optional<Uuid> from_bytes(
			std::span<const u8, ByteCount> bytes) noexcept;

		[[nodiscard]] constexpr std::span<const u8, ByteCount> bytes() const noexcept
		{
			return mBytes;
		}

		[[nodiscard]] constexpr std::array<u8, ByteCount> to_array() const noexcept
		{
			return mBytes;
		}

		[[nodiscard]] constexpr bool is_nil() const noexcept
		{
			for (const u8 byte : mBytes)
			{
				if (byte != 0) return false;
			}
			return true;
		}

		[[nodiscard]] constexpr u8 version() const noexcept
		{
			return static_cast<u8>((mBytes[6] >> 4) & 0x0F);
		}

		[[nodiscard]] std::string to_string() const;

		friend constexpr bool operator==(const Uuid&, const Uuid&) noexcept = default;
		friend constexpr auto operator<=>(const Uuid&, const Uuid&) noexcept = default;

	private:
		std::array<u8, ByteCount> mBytes{};
	};

	static_assert(sizeof(Uuid) == Uuid::ByteCount);
	static_assert(std::is_trivially_copyable_v<Uuid>);
}

template <>
struct std::formatter<wcore::Uuid>
{
	constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

	auto format(const wcore::Uuid& uuid, std::format_context& ctx) const
	{
		return std::format_to(ctx.out(), "{}", uuid.to_string());
	}
};

template <>
struct std::hash<wcore::Uuid>
{
	std::size_t operator()(const wcore::Uuid& uuid) const noexcept
	{
		// FNV-1a over the 16 bytes.
		u64 hash{ 0xcbf29ce484222325ULL };
		for (const u8 byte : uuid.bytes())
		{
			hash ^= byte;
			hash *= 0x100000001b3ULL;
		}
		return static_cast<std::size_t>(hash);
	}
};
