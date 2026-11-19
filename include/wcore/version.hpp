#pragma once

#include <compare>
#include <iosfwd>
#include <string>
#include <string_view>

#include <wcore/version.generated.hpp>

namespace wcore
{
	struct Version
	{
		unsigned major;
		unsigned minor;
		unsigned patch;

		constexpr auto operator<=>(const Version&) const = default;
	};

	inline constexpr Version CurrentVersion{
		WCORE_VERSION_MAJOR,
		WCORE_VERSION_MINOR,
		WCORE_VERSION_PATCH,
	};
	inline constexpr std::string_view CurrentVersionString{ WCORE_VERSION_STRING };

	Version engine_version() noexcept;
	const char* version_str() noexcept;
	std::string_view version_string() noexcept;

	std::string to_string(const Version& v);
	std::ostream& operator<<(std::ostream& os, const Version& v);
} // namespace wcore
