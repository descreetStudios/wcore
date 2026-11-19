#include <wcore/version.hpp>

#include <ostream>

namespace wcore
{
	Version engine_version() noexcept
	{
		return CurrentVersion;
	}

	const char* version_str() noexcept
	{
		return WCORE_VERSION_STRING;
	}

	std::string_view version_string() noexcept
	{
		return CurrentVersionString;
	}

	std::string to_string(const Version& v)
	{
		using std::to_string;
		return to_string(v.major) + '.' +
			to_string(v.minor) + '.' + to_string(v.patch);
	}

	std::ostream& operator<<(std::ostream& os, const Version& v)
	{
		return os << to_string(v);
	}
} // namespace wcore
