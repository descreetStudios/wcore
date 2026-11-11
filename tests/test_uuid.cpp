#include <wcore/test.hpp>
#include <wcore/uuid.hpp>

#include <format>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

using namespace wcore;
using namespace test;

namespace
{
	Uuid make_uuid(std::array<u8, Uuid::ByteCount> bytes)
	{
		return Uuid{ bytes };
	}

	Uuid parse(std::string_view text)
	{
		const std::optional<Uuid> uuid{ Uuid::from_string(text) };
		REQUIRE(uuid.has_value());
		return *uuid;
	}
}

TEST(uuid_default_is_nil)
{
	const Uuid uuid;
	CHECK(uuid.is_nil());
	CHECK_EQ(uuid.version(), 0u);
	CHECK_EQ(uuid.to_string(),
		std::string("00000000-0000-0000-0000-000000000000"));
	CHECK(Uuid::nil() == uuid);
}

TEST(uuid_generate_produces_v4_values)
{
	const Uuid uuid{ Uuid::generate() };

	CHECK(!uuid.is_nil());
	CHECK_EQ(uuid.version(), 4u);

	// RFC 4122 variant: the two most significant bits are 10.
	CHECK_EQ(static_cast<u32>(uuid.bytes()[8] >> 6), 2u);
}

TEST(uuid_generate_does_not_repeat)
{
	std::unordered_set<Uuid> seen;

	for (int i{ 0 }; i < 1000; ++i)
	{
		const Uuid uuid{ Uuid::generate() };
		CHECK(seen.insert(uuid).second);
	}

	CHECK_EQ(seen.size(), 1000u);
}

TEST(uuid_generate_is_unique_across_threads)
{
	constexpr int ThreadCount{ 4 };
	constexpr int PerThread{ 250 };

	std::vector<Uuid> generated(ThreadCount * PerThread);
	std::vector<std::thread> threads;

	for (int thread{ 0 }; thread < ThreadCount; ++thread)
	{
		threads.emplace_back([&generated, thread]
		{
			for (int i{ 0 }; i < PerThread; ++i)
				generated[thread * PerThread + i] = Uuid::generate();
		});
	}

	for (std::thread& thread : threads)
		thread.join();

	const std::unordered_set<Uuid> unique(generated.begin(), generated.end());
	CHECK_EQ(unique.size(), generated.size());
}

TEST(uuid_to_string_is_canonical)
{
	const Uuid uuid{ make_uuid({ 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
		0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff }) };

	const std::string text{ uuid.to_string() };
	CHECK_EQ(text.size(), Uuid::StringLength);
	CHECK_EQ(text, std::string("00112233-4455-6677-8899-aabbccddeeff"));

	CHECK_EQ(text[8], '-');
	CHECK_EQ(text[13], '-');
	CHECK_EQ(text[18], '-');
	CHECK_EQ(text[23], '-');
}

TEST(uuid_from_string_accepts_canonical_form)
{
	const Uuid uuid{ parse("00112233-4455-6677-8899-aabbccddeeff") };

	CHECK_EQ(uuid.to_string(),
		std::string("00112233-4455-6677-8899-aabbccddeeff"));
}

TEST(uuid_from_string_accepts_uppercase)
{
	const Uuid uuid{ parse("00112233-4455-6677-8899-AABBCCDDEEFF") };

	CHECK_EQ(uuid.to_string(),
		std::string("00112233-4455-6677-8899-aabbccddeeff"));
}

TEST(uuid_from_string_accepts_compact_form)
{
	const Uuid uuid{ parse("00112233445566778899aabbccddeeff") };

	CHECK_EQ(uuid.to_string(),
		std::string("00112233-4455-6677-8899-aabbccddeeff"));
}

TEST(uuid_from_string_round_trips_generated_values)
{
	const Uuid original{ Uuid::generate() };
	const Uuid parsed{ parse(original.to_string()) };

	CHECK(original == parsed);
}

TEST(uuid_from_string_rejects_malformed_input)
{
	CHECK(!Uuid::from_string("").has_value());
	CHECK(!Uuid::from_string("00112233").has_value());
	CHECK(!Uuid::from_string("00112233-4455-6677-8899-aabbccddeef").has_value());
	CHECK(!Uuid::from_string("00112233-4455-6677-8899-aabbccddeeff0").has_value());
	CHECK(!Uuid::from_string("00112233x4455-6677-8899-aabbccddeeff").has_value());
	CHECK(!Uuid::from_string("00112233445566778899aabbccddeefg").has_value());
	CHECK(!Uuid::from_string(
		"00112233-4455-6677-8899-aabbccddeefg").has_value());
}

TEST(uuid_from_bytes_round_trips)
{
	const std::array<u8, Uuid::ByteCount> bytes{ 0xde, 0xad, 0xbe, 0xef,
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
		0x0c };

	const std::optional<Uuid> uuid{ Uuid::from_bytes(bytes) };
	REQUIRE(uuid.has_value());
	CHECK(uuid->to_array() == bytes);
	CHECK_EQ(uuid->bytes().size(), Uuid::ByteCount);
}

TEST(uuid_equality_and_ordering)
{
	const Uuid low{ make_uuid({ 1 }) };
	const Uuid high{ make_uuid({ 2 }) };

	CHECK(low == low);
	CHECK(low != high);
	CHECK(low < high);
	CHECK(high > low);
	CHECK(!(low < low));
}

TEST(uuid_formats)
{
	const Uuid uuid{ make_uuid({ 0xab }) };
	CHECK_EQ(std::format("{}", uuid),
		std::string("ab000000-0000-0000-0000-000000000000"));
}

TEST(uuid_hash_is_stable_for_equal_values)
{
	const std::hash<Uuid> hash;
	const Uuid a{ Uuid::generate() };
	const Uuid b{ parse(a.to_string()) };

	CHECK_EQ(hash(a), hash(b));
}
