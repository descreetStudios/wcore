#include <wcore/version.hpp>

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>

namespace
{
	constexpr bool matches_macros(const wcore::Version& v)
	{
		return v.major == WCORE_VERSION_MAJOR
			&& v.minor == WCORE_VERSION_MINOR
			&& v.patch == WCORE_VERSION_PATCH;
	}

	static_assert(matches_macros(wcore::CurrentVersion));
	static_assert(wcore::CurrentVersion == wcore::Version{
		WCORE_VERSION_MAJOR, WCORE_VERSION_MINOR, WCORE_VERSION_PATCH });
	static_assert(wcore::CurrentVersionString == WCORE_VERSION_STRING);
} // namespace

TEST_CASE("version is available through macros", "[version]")
{
	REQUIRE(WCORE_VERSION_MAJOR == WCORE_EXPECTED_VERSION_MAJOR);
	REQUIRE(WCORE_VERSION_MINOR == WCORE_EXPECTED_VERSION_MINOR);
	REQUIRE(WCORE_VERSION_PATCH == WCORE_EXPECTED_VERSION_PATCH);
	REQUIRE(std::string{ WCORE_VERSION_STRING } == WCORE_EXPECTED_VERSION);
}

TEST_CASE("version is available through constants", "[version]")
{
	REQUIRE(wcore::CurrentVersion.major == WCORE_VERSION_MAJOR);
	REQUIRE(wcore::CurrentVersion.minor == WCORE_VERSION_MINOR);
	REQUIRE(wcore::CurrentVersion.patch == WCORE_VERSION_PATCH);
	REQUIRE(std::string{ wcore::CurrentVersionString } == WCORE_EXPECTED_VERSION);
}

TEST_CASE("version is available through runtime functions", "[version]")
{
	REQUIRE(wcore::engine_version() == wcore::CurrentVersion);
	REQUIRE(std::string{ wcore::version_str() } == WCORE_EXPECTED_VERSION);
	REQUIRE(std::string{ wcore::version_string() } == WCORE_EXPECTED_VERSION);
}

TEST_CASE("version is available through formatting", "[version]")
{
	REQUIRE(wcore::to_string(wcore::CurrentVersion) == WCORE_EXPECTED_VERSION);

	std::ostringstream out;
	out << wcore::CurrentVersion;
	REQUIRE(out.str() == WCORE_EXPECTED_VERSION);
}

TEST_CASE("versions compare semantically", "[version]")
{
	REQUIRE(wcore::Version{ 1, 0, 0 } > wcore::Version{ 0, 9, 9 });
	REQUIRE(wcore::Version{ 1, 2, 3 } == wcore::Version{ 1, 2, 3 });
	REQUIRE(wcore::CurrentVersion >= wcore::Version{ 0, 0, 0 });
}
