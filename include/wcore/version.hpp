#pragma once

#include <string>
#include <string_view>

#include <wcore/types.hpp>
#include <wcore/version.generated.hpp>

namespace wcore
{
	struct Version
	{
		u32 major;
		u32 minor;
		u32 patch;

		constexpr auto operator<=>(const Version&) const = default;
	};

	inline constexpr Version CurrentVersion{
		WCORE_VERSION_MAJOR,
		WCORE_VERSION_MINOR,
		WCORE_VERSION_PATCH,
	};
	inline constexpr std::string_view CurrentVersionString{ WCORE_VERSION_STRING };

	Version version() noexcept;
	const char* version_str() noexcept;
	std::string_view version_string() noexcept;

	std::string to_string(const Version& v);
	std::ostream& operator<<(std::ostream& os, const Version& v);
} // namespace wcore
