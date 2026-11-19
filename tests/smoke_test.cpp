#include <catch2/catch_test_macros.hpp>

static_assert(__cpp_concepts >= 201907L, "expected C++20 to be enabled");

TEST_CASE("test harness is wired up", "[smoke]")
{
	REQUIRE(1 + 1 == 2);
}