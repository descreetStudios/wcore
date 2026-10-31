#include <wcore/test.hpp>
#include <wcore/binary.hpp>

#include <cstddef>
#include <initializer_list>
#include <limits>
#include <string>
#include <vector>

using namespace wcore;
using namespace test;

namespace
{
	binary::Bytes make_bytes(std::initializer_list<unsigned> values)
	{
		binary::Bytes result;
		result.reserve(values.size());

		for (const unsigned value : values)
			result.push_back(static_cast<binary::Byte>(value));

		return result;
	}

	bool same_bytes(const binary::Bytes& bytes,
		std::initializer_list<unsigned> expected)
	{
		if (bytes.size() != expected.size())
			return false;

		std::size_t i = 0;
		for (const unsigned value : expected)
		{
			if (std::to_integer<unsigned>(bytes[i]) != value)
				return false;

			++i;
		}

		return true;
	}

	enum class Color : u16
	{
		Red = 1,
		Blue = 300
	};

	struct Pair
	{
		u32 x;
		u32 y;
	};
}

TEST(binary_writer_little_endian_bytes)
{
	binary::Writer writer;
	writer.write<u16>(0x1234);
	writer.write<u32>(0x01020304);

	REQUIRE(writer.size() == 6);
	CHECK(same_bytes(writer.data(),
		{ 0x34, 0x12, 0x04, 0x03, 0x02, 0x01 }));
}

TEST(binary_writer_big_endian_bytes)
{
	binary::Writer writer;
	writer.write_big<u32>(0x01020304);

	REQUIRE(writer.size() == 4);
	CHECK(same_bytes(writer.data(), { 0x01, 0x02, 0x03, 0x04 }));
}

TEST(binary_scalar_round_trip)
{
	binary::Writer writer;
	writer.write<u8>(0x7F);
	writer.write<u16>(0xBEEF);
	writer.write<u32>(0xDEADBEEF);
	writer.write<u64>(0x0123456789ABCDEF);
	writer.write<i32>(-123456);
	writer.write<i64>(-9876543210);
	writer.write<float>(3.5f);
	writer.write<double>(-1234.25);

	binary::Reader reader(writer.view());

	const auto u8Value = reader.read<u8>();
	const auto u16Value = reader.read<u16>();
	const auto u32Value = reader.read<u32>();
	const auto u64Value = reader.read<u64>();
	const auto i32Value = reader.read<i32>();
	const auto i64Value = reader.read<i64>();
	const auto f32Value = reader.read<float>();
	const auto f64Value = reader.read<double>();

	REQUIRE(u8Value.has_value());
	REQUIRE(u16Value.has_value());
	REQUIRE(u32Value.has_value());
	REQUIRE(u64Value.has_value());
	REQUIRE(i32Value.has_value());
	REQUIRE(i64Value.has_value());
	REQUIRE(f32Value.has_value());
	REQUIRE(f64Value.has_value());

	CHECK_EQ(*u8Value, u8{ 0x7F });
	CHECK_EQ(*u16Value, u16{ 0xBEEF });
	CHECK_EQ(*u32Value, u32{ 0xDEADBEEF });
	CHECK_EQ(*u64Value, u64{ 0x0123456789ABCDEF });
	CHECK_EQ(*i32Value, i32{ -123456 });
	CHECK_EQ(*i64Value, i64{ -9876543210 });
	CHECK_EQ(*f32Value, 3.5f);
	CHECK_EQ(*f64Value, -1234.25);

	CHECK(!reader.failed());
	CHECK(reader.empty());
}

TEST(binary_big_endian_round_trip)
{
	binary::Writer writer;
	writer.write_big<u16>(0x1234);
	writer.write_big<u64>(0x0102030405060708);

	binary::Reader reader(writer.view());

	const auto u16Value = reader.read_big<u16>();
	const auto u64Value = reader.read_big<u64>();

	REQUIRE(u16Value.has_value());
	REQUIRE(u64Value.has_value());
	CHECK_EQ(*u16Value, u16{ 0x1234 });
	CHECK_EQ(*u64Value, u64{ 0x0102030405060708 });
	CHECK(reader.empty());
}

TEST(binary_bool_round_trip)
{
	binary::Writer writer;
	writer.write(true);
	writer.write(false);

	REQUIRE(writer.size() == 2);
	CHECK(same_bytes(writer.data(), { 1, 0 }));

	binary::Reader reader(writer.view());
	const auto first = reader.read<bool>();
	const auto second = reader.read<bool>();

	REQUIRE(first.has_value());
	REQUIRE(second.has_value());
	CHECK(*first);
	CHECK(!*second);
}

TEST(binary_enum_round_trip)
{
	binary::Writer writer;
	writer.write(Color::Blue);

	binary::Reader reader(writer.view());
	const auto value = reader.read<Color>();

	REQUIRE(value.has_value());
	CHECK(*value == Color::Blue);
	CHECK(reader.empty());
}

TEST(binary_pod_round_trip)
{
	const Pair source{ 0x11223344, 0xAABBCCDD };

	binary::Writer writer;
	writer.write_pod(source);
	REQUIRE(writer.size() == sizeof(Pair));

	binary::Reader reader(writer.view());
	Pair dest{};
	CHECK(reader.read_pod(dest));
	CHECK_EQ(dest.x, source.x);
	CHECK_EQ(dest.y, source.y);
	CHECK(reader.empty());
}

TEST(binary_write_and_read_bytes)
{
	const binary::Bytes payload = make_bytes({ 1, 2, 3, 4, 5 });

	binary::Writer writer;
	writer.write_bytes(payload);

	binary::Reader reader(writer.view());
	const auto bytes = reader.read_bytes(3);
	REQUIRE(bytes.has_value());
	CHECK_EQ(bytes->size(), 3u);
	CHECK_EQ(std::to_integer<unsigned>((*bytes)[0]), 1u);
	CHECK_EQ(std::to_integer<unsigned>((*bytes)[2]), 3u);
	CHECK_EQ(reader.remaining(), 2u);

	const auto rest = reader.read_bytes(2);
	REQUIRE(rest.has_value());
	CHECK_EQ(std::to_integer<unsigned>((*rest)[1]), 5u);
	CHECK(reader.empty());
}

TEST(binary_string_round_trip)
{
	binary::Writer writer;
	writer.write_string("hello");

	// u32 length prefix followed by the raw bytes.
	REQUIRE(writer.size() == 9);
	CHECK(same_bytes(writer.data(),
		{ 5, 0, 0, 0, 'h', 'e', 'l', 'l', 'o' }));

	binary::Reader reader(writer.view());
	const auto text = reader.read_string();

	REQUIRE(text.has_value());
	CHECK_EQ(*text, std::string("hello"));
	CHECK(reader.empty());
}

TEST(binary_empty_string_round_trip)
{
	binary::Writer writer;
	writer.write_string("");

	REQUIRE(writer.size() == 4);
	CHECK(same_bytes(writer.data(), { 0, 0, 0, 0 }));

	binary::Reader reader(writer.view());
	const auto text = reader.read_string();

	REQUIRE(text.has_value());
	CHECK(text->empty());
	CHECK(reader.empty());
}

TEST(binary_varuint_encoding)
{
	{
		binary::Writer writer;
		writer.write_varuint(0);
		CHECK(same_bytes(writer.data(), { 0x00 }));
	}
	{
		binary::Writer writer;
		writer.write_varuint(127);
		CHECK(same_bytes(writer.data(), { 0x7F }));
	}
	{
		binary::Writer writer;
		writer.write_varuint(128);
		CHECK(same_bytes(writer.data(), { 0x80, 0x01 }));
	}
	{
		binary::Writer writer;
		writer.write_varuint(300);
		CHECK(same_bytes(writer.data(), { 0xAC, 0x02 }));
	}
	{
		binary::Writer writer;
		writer.write_varuint(std::numeric_limits<u64>::max());
		CHECK(same_bytes(writer.data(), {
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0x01 }));
	}
}

TEST(binary_varuint_round_trip)
{
	const u64 values[]{
		0, 1, 2, 127, 128, 300, 16383, 16384,
		(1ull << 32), std::numeric_limits<u64>::max() };

	for (const u64 value : values)
	{
		binary::Writer writer;
		writer.write_varuint(value);

		binary::Reader reader(writer.view());
		const auto decoded = reader.read_varuint();

		REQUIRE(decoded.has_value());
		CHECK_EQ(*decoded, value);
		CHECK(reader.empty());
	}
}

TEST(binary_varint_round_trip)
{
	const i64 values[]{
		0, 1, -1, 2, -2, 127, -128, 123456789, -123456789,
		std::numeric_limits<i64>::max(), std::numeric_limits<i64>::min() };

	for (const i64 value : values)
	{
		binary::Writer writer;
		writer.write_varint(value);

		binary::Reader reader(writer.view());
		const auto decoded = reader.read_varint();

		REQUIRE(decoded.has_value());
		CHECK_EQ(*decoded, value);
		CHECK(reader.empty());
	}
}

TEST(binary_zigzag)
{
	CHECK_EQ(binary::zigzag_encode(0), u64{ 0 });
	CHECK_EQ(binary::zigzag_encode(-1), u64{ 1 });
	CHECK_EQ(binary::zigzag_encode(1), u64{ 2 });
	CHECK_EQ(binary::zigzag_encode(-2), u64{ 3 });

	CHECK_EQ(binary::zigzag_decode(0), i64{ 0 });
	CHECK_EQ(binary::zigzag_decode(1), i64{ -1 });
	CHECK_EQ(binary::zigzag_decode(2), i64{ 1 });
	CHECK_EQ(binary::zigzag_decode(3), i64{ -2 });

	CHECK_EQ(binary::zigzag_encode(std::numeric_limits<i64>::max()),
		std::numeric_limits<u64>::max() - 1);
	CHECK_EQ(binary::zigzag_encode(std::numeric_limits<i64>::min()),
		std::numeric_limits<u64>::max());

	CHECK_EQ(binary::zigzag_decode(
		binary::zigzag_encode(-123456789)), i64{ -123456789 });
}

TEST(binary_reader_reports_truncated_input)
{
	binary::Writer writer;
	writer.write<u32>(0x11223344);

	const binary::Bytes truncated{ writer.data().begin(),
		writer.data().begin() + 3 };

	binary::Reader reader(truncated);
	CHECK(!reader.read<u32>().has_value());
	CHECK(reader.failed());
	CHECK_EQ(reader.remaining(), 3u);
}

TEST(binary_reader_stays_failed)
{
	binary::Reader reader;

	CHECK(!reader.failed());
	CHECK(!reader.read<u8>().has_value());
	CHECK(reader.failed());

	// Even data cannot be read until the error is cleared.
	CHECK(!reader.read<u8>().has_value());

	reader.clear_error();
	CHECK(!reader.failed());
	CHECK(!reader.read<u8>().has_value());
	CHECK(reader.failed());
}

TEST(binary_reader_skip)
{
	const binary::Bytes data = make_bytes({ 1, 2, 3, 4 });

	binary::Reader reader(data);
	CHECK(reader.skip(2));
	CHECK_EQ(reader.remaining(), 2u);
	CHECK_EQ(reader.position(), 2u);
	CHECK_EQ(std::to_integer<unsigned>(reader.view()[0]), 3u);

	CHECK(!reader.skip(3));
	CHECK(reader.failed());
}

TEST(binary_reader_rejects_bad_varuint)
{
	{
		// Eleven continuation bytes cannot fit in a u64.
		const binary::Bytes data(11, binary::Byte{ 0xFF });

		binary::Reader reader(data);
		CHECK(!reader.read_varuint().has_value());
		CHECK(reader.failed());
	}
	{
		// The tenth byte may only contribute one bit.
		const binary::Bytes data = make_bytes({
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0x02 });

		binary::Reader reader(data);
		CHECK(!reader.read_varuint().has_value());
		CHECK(reader.failed());
	}
}

TEST(binary_reader_rejects_bad_string_length)
{
	// Says four bytes follow, but only one is present.
	const binary::Bytes data = make_bytes({ 4, 0, 0, 0, 'a' });

	binary::Reader reader(data);
	CHECK(!reader.read_string().has_value());
	CHECK(reader.failed());
}

TEST(binary_reader_reset)
{
	binary::Writer writer;
	writer.write<u16>(0x1234);

	binary::Reader reader;
	CHECK(!reader.read<u16>().has_value());
	CHECK(reader.failed());

	reader.reset(writer.view());
	CHECK(!reader.failed());
	CHECK_EQ(reader.remaining(), 2u);

	const auto value = reader.read<u16>();
	REQUIRE(value.has_value());
	CHECK_EQ(*value, u16{ 0x1234 });
}

TEST(binary_writer_clear_and_release)
{
	binary::Writer writer(16);

	writer.write<u32>(1);
	CHECK_EQ(writer.size(), 4u);
	CHECK(!writer.empty());

	writer.clear();
	CHECK(writer.empty());
	CHECK_EQ(writer.size(), 0u);

	writer.write<u8>(9);
	binary::Bytes bytes = writer.release();

	REQUIRE(bytes.size() == 1);
	CHECK_EQ(std::to_integer<unsigned>(bytes[0]), 9u);
	CHECK(writer.empty());
}

TEST(binary_endian_helpers)
{
	const u16 value = 0x1234;
	const u16 swapped = binary::byte_swap(value);

	CHECK_EQ(binary::byte_swap(swapped), value);
	CHECK_EQ(binary::from_little_endian(binary::to_little_endian(value)),
		value);
	CHECK_EQ(binary::from_big_endian(binary::to_big_endian(value)),
		value);

	const double number = 3.5;
	CHECK_EQ(binary::from_little_endian(binary::to_little_endian(number)),
		number);
}
