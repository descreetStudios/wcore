#include <wcore/types.hpp>

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

TEST_CASE("integer aliases map to fixed-width types", "[types]")
{
	STATIC_REQUIRE(std::is_same_v<u8, std::uint8_t>);
	STATIC_REQUIRE(std::is_same_v<u16, std::uint16_t>);
	STATIC_REQUIRE(std::is_same_v<u32, std::uint32_t>);
	STATIC_REQUIRE(std::is_same_v<u64, std::uint64_t>);

	STATIC_REQUIRE(std::is_same_v<i8, std::int8_t>);
	STATIC_REQUIRE(std::is_same_v<i16, std::int16_t>);
	STATIC_REQUIRE(std::is_same_v<i32, std::int32_t>);
	STATIC_REQUIRE(std::is_same_v<i64, std::int64_t>);
}

TEST_CASE("integer aliases have the expected width and signedness", "[types]")
{
	STATIC_REQUIRE(sizeof(u8) == 1);
	STATIC_REQUIRE(sizeof(u16) == 2);
	STATIC_REQUIRE(sizeof(u32) == 4);
	STATIC_REQUIRE(sizeof(u64) == 8);

	STATIC_REQUIRE(sizeof(i8) == 1);
	STATIC_REQUIRE(sizeof(i16) == 2);
	STATIC_REQUIRE(sizeof(i32) == 4);
	STATIC_REQUIRE(sizeof(i64) == 8);

	STATIC_REQUIRE(std::is_unsigned_v<u8>);
	STATIC_REQUIRE(std::is_unsigned_v<u16>);
	STATIC_REQUIRE(std::is_unsigned_v<u32>);
	STATIC_REQUIRE(std::is_unsigned_v<u64>);

	STATIC_REQUIRE(std::is_signed_v<i8>);
	STATIC_REQUIRE(std::is_signed_v<i16>);
	STATIC_REQUIRE(std::is_signed_v<i32>);
	STATIC_REQUIRE(std::is_signed_v<i64>);
}

TEST_CASE("floating point aliases map to native types", "[types]")
{
	STATIC_REQUIRE(std::is_same_v<f32, float>);
	STATIC_REQUIRE(std::is_same_v<f64, double>);

	STATIC_REQUIRE(sizeof(f32) == 4);
	STATIC_REQUIRE(sizeof(f64) == 8);

	STATIC_REQUIRE(std::is_floating_point_v<f32>);
	STATIC_REQUIRE(std::is_floating_point_v<f64>);
}
