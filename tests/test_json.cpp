#include <wcore/test.hpp>
#include <wcore/json.hpp>

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

using namespace wcore;
using namespace test;

namespace
{
	json::Value parse_ok(std::string_view text)
	{
		json::ParseError error;
		auto value = json::parse(text, &error);

		REQUIRE(value.has_value());
		return std::move(*value);
	}
}

TEST(json_parse_scalars)
{
	CHECK(parse_ok("null").is_null());
	CHECK(parse_ok("true").is_bool());
	CHECK(parse_ok("true").as_bool());
	CHECK(!parse_ok("false").as_bool());
	CHECK_EQ(parse_ok("42").as_i64(), i64{ 42 });
	CHECK_EQ(parse_ok("-7").as_i64(), i64{ -7 });
	CHECK(parse_ok("42").is_integer());
	CHECK(parse_ok("42").is_number());
}

TEST(json_parse_integer_boundaries)
{
	CHECK_EQ(parse_ok("9223372036854775807").as_i64(),
		std::numeric_limits<i64>::max());
	CHECK_EQ(parse_ok("-9223372036854775808").as_i64(),
		std::numeric_limits<i64>::min());

	// Values outside i64 fall back to double instead of wrapping.
	const auto big = parse_ok("9223372036854775808");
	CHECK(big.is_number());
	CHECK(!big.is_integer());
	CHECK(big.as_double() > 9.2e18);
}

TEST(json_parse_numbers)
{
	const auto decimal{ parse_ok("3.5") };
	CHECK(decimal.is_number());
	CHECK(!decimal.is_integer());
	CHECK(std::abs(decimal.as_double() - 3.5) < 1e-12);

	CHECK(std::abs(parse_ok("-0.25").as_double() + 0.25) < 1e-12);
	CHECK(std::abs(parse_ok("1e3").as_double() - 1000.0) < 1e-9);
	CHECK(std::abs(parse_ok("2.5E-2").as_double() - 0.025) < 1e-12);
	CHECK(std::abs(parse_ok("0").as_double()) < 1e-12);
	CHECK(std::abs(parse_ok("-0").as_double()) < 1e-12);
}

TEST(json_rejects_invalid_numbers)
{
	const char* invalid[]{
		"01", "1.", ".5", "+1", "1e", "1e+", "--1",
		"NaN", "Infinity", "1e400", "-" };

	for (const char* text : invalid)
	{
		json::ParseError error;
		CHECK(!json::parse(text, &error).has_value());
		CHECK(!error.message.empty());
	}
}

TEST(json_parse_strings)
{
	CHECK_EQ(parse_ok("\"hello\"").as_string(), std::string("hello"));
	CHECK_EQ(parse_ok("\"\"").as_string(), std::string(""));

	CHECK_EQ(parse_ok("\"a\\nb\\t\\\\\\\"\"").as_string(),
		std::string("a\nb\t\\\""));

	CHECK_EQ(parse_ok("\"\\/\"").as_string(), std::string("/"));
	CHECK_EQ(parse_ok("\"\\b\\f\\r\"").as_string(),
		std::string("\b\f\r"));
}

TEST(json_parse_unicode_escapes)
{
	CHECK_EQ(parse_ok("\"\\u0041\\u00e9\"").as_string(),
		std::string("A\xC3\xA9"));

	// Surrogate pair for U+1F600.
	CHECK_EQ(parse_ok("\"\\uD83D\\uDE00\"").as_string(),
		std::string("\xF0\x9F\x98\x80"));

	// Escaped NUL is preserved as an embedded zero byte.
	const auto nul = parse_ok("\"a\\u0000b\"");
	CHECK_EQ(nul.as_string().size(), 3u);
	CHECK_EQ(nul.as_string()[1], '\0');
}

TEST(json_rejects_invalid_strings)
{
	const char* invalid[]{
		"\"abc",
		"\"\\q\"",
		"\"\\u12\"",
		"\"\\uD800\"",
		"\"\\uDC00\"",
		"\"\\uD800\\u0041\"",
		"\"a\nb\"" };

	for (const char* text : invalid)
	{
		json::ParseError error;
		CHECK(!json::parse(text, &error).has_value());
		CHECK(!error.message.empty());
	}
}

TEST(json_parse_objects)
{
	const auto value = parse_ok(
		R"({"name":"wcore","version":1,"tags":["a","b"],)"
		R"("nested":{"ok":true},"empty":[]})");

	REQUIRE(value.is_object());
	CHECK_EQ(value.size(), 5u);
	CHECK_EQ(value["name"].as_string(), std::string("wcore"));
	CHECK_EQ(value["version"].as_i64(), i64{ 1 });

	REQUIRE(value["tags"].is_array());
	CHECK_EQ(value["tags"].size(), 2u);
	CHECK_EQ(value["tags"][0].as_string(), std::string("a"));
	CHECK_EQ(value["tags"][1].as_string(), std::string("b"));

	CHECK(value["nested"]["ok"].as_bool());
	CHECK(value["empty"].is_array());
	CHECK(value["empty"].empty());

	CHECK(value.contains("name"));
	CHECK(!value.contains("missing"));
	CHECK(value.find("name") != nullptr);
	CHECK(value.find("missing") == nullptr);
}

TEST(json_parse_duplicate_keys_last_wins)
{
	const auto value = parse_ok(R"({"a":1,"b":2,"a":3})");

	CHECK_EQ(value.size(), 2u);
	CHECK_EQ(value["a"].as_i64(), i64{ 3 });
}

TEST(json_parse_empty_and_whitespace)
{
	CHECK(parse_ok("{}").is_object());
	CHECK(parse_ok("[]").is_array());
	CHECK(parse_ok(" \r\n\t[ 1 , 2 ] \n").is_array());
	CHECK_EQ(parse_ok(" \r\n\t[ 1 , 2 ] \n").size(), 2u);
}

TEST(json_parse_utf8_bom)
{
	const auto value = parse_ok("\xEF\xBB\xBF{\"a\":1}");

	CHECK(value.is_object());
	CHECK_EQ(value["a"].as_i64(), i64{ 1 });
}

TEST(json_parse_rejects_trailing_content)
{
	json::ParseError error;
	CHECK(!json::parse("{} garbage", &error).has_value());
	CHECK_EQ(error.offset, 3u);
	CHECK(!error.message.empty());
}

TEST(json_parse_rejects_bad_containers)
{
	const char* invalid[]{
		"[1,]",
		"{\"a\":1,}",
		"{\"a\" 1}",
		"[1 2]",
		"{\"a\":}",
		"[",
		"{",
		"[,]",
		"{\"a\":1" };

	for (const char* text : invalid)
	{
		json::ParseError error;
		CHECK(!json::parse(text, &error).has_value());
		CHECK(!error.message.empty());
	}
}

TEST(json_parse_error_location)
{
	json::ParseError error;
	CHECK(!json::parse("{\n  \"a\": 1,\n}", &error).has_value());
	CHECK_EQ(error.line, 3u);
	CHECK_EQ(error.column, 1u);
	CHECK_EQ(error.offset, 12u);
}

TEST(json_parse_depth_limit)
{
	std::string deep(600, '[');
	deep.append(600, ']');

	json::ParseError error;
	CHECK(!json::parse(deep, &error).has_value());
	CHECK(!error.message.empty());

	std::string allowed(512, '[');
	allowed.append(512, ']');
	CHECK(json::parse(allowed).has_value());
}

TEST(json_dump_scalars)
{
	CHECK_EQ(json::dump(json::Value()), std::string("null"));
	CHECK_EQ(json::dump(json::Value(true)), std::string("true"));
	CHECK_EQ(json::dump(json::Value(false)), std::string("false"));
	CHECK_EQ(json::dump(json::Value(42)), std::string("42"));
	CHECK_EQ(json::dump(json::Value(-42)), std::string("-42"));
	CHECK_EQ(json::dump(json::Value(3.5)), std::string("3.5"));
	CHECK_EQ(json::dump(json::Value(1.0)), std::string("1.0"));
	CHECK_EQ(json::dump(json::Value(-0.0)), std::string("-0.0"));
	CHECK_EQ(json::dump(json::Value("hi")), std::string("\"hi\""));
	CHECK_EQ(json::dump(json::Value(std::string("hi"))),
		std::string("\"hi\""));
}

TEST(json_dump_non_finite_numbers_as_null)
{
	const double inf{ std::numeric_limits<double>::infinity() };
	const double nan{ std::numeric_limits<double>::quiet_NaN() };

	CHECK_EQ(json::dump(json::Value(inf)), std::string("null"));
	CHECK_EQ(json::dump(json::Value(-inf)), std::string("null"));
	CHECK_EQ(json::dump(json::Value(nan)), std::string("null"));
}

TEST(json_dump_escapes_strings)
{
	const std::string text{ "a\"b\\c\n\t\r\b\f\x01" };
	const std::string expected{ "\"a\\\"b\\\\c\\n\\t\\r\\b\\f\\u0001\"" };

	CHECK_EQ(json::dump(json::Value(text)), expected);
}

TEST(json_dump_objects_and_arrays)
{
	const auto value = parse_ok(
		R"({"b":2,"a":[1,true,null,"x"],"empty":{}})");

	CHECK_EQ(json::dump(value),
		std::string(R"({"a":[1,true,null,"x"],"b":2,"empty":{}})"));
	CHECK_EQ(json::dump(parse_ok("[]")), std::string("[]"));
	CHECK_EQ(json::dump(parse_ok("{}")), std::string("{}"));
}

TEST(json_dump_pretty)
{
	const auto value = parse_ok(R"({"a":[1,2]})");

	const std::string expected{
		"{\n"
		"  \"a\": [\n"
		"    1,\n"
		"    2\n"
		"  ]\n"
		"}" };

	CHECK_EQ(json::dump(value, true), expected);

	const std::string wide{
		"{\n"
		"    \"a\": [\n"
		"        1,\n"
		"        2\n"
		"    ]\n"
		"}" };

	CHECK_EQ(json::dump(value, json::DumpOptions{ true, 4 }), wide);
}

TEST(json_dump_round_trip)
{
	const auto original = parse_ok(
		R"({"a":[1,2.5,true,null,"x\n"],"b":{"c":-3}})");

	const std::string canonical{ json::dump(original) };
	const auto reparsed = parse_ok(canonical);

	CHECK_EQ(json::dump(reparsed), canonical);
	CHECK_EQ(reparsed["a"][1].as_double(), 2.5);
	CHECK(reparsed["a"][2].as_bool());
	CHECK(reparsed["a"][3].is_null());
	CHECK_EQ(reparsed["a"][4].as_string(), std::string("x\n"));
	CHECK_EQ(reparsed["b"]["c"].as_i64(), i64{ -3 });
}

TEST(json_value_deep_copy)
{
	json::Value original{ json::Value::array() };
	original.push_back(1);
	original.push_back(json::Value::object());

	json::Value copy{ original };
	copy[0] = 2;
	copy[1]["x"] = 3;

	CHECK_EQ(original[0].as_i64(), i64{ 1 });
	CHECK(!original[1].contains("x"));
	CHECK_EQ(copy[0].as_i64(), i64{ 2 });
	CHECK_EQ(copy[1]["x"].as_i64(), i64{ 3 });

	json::Value assigned;
	assigned = original;
	assigned[0] = 9;

	CHECK_EQ(original[0].as_i64(), i64{ 1 });
	CHECK_EQ(assigned[0].as_i64(), i64{ 9 });
}

TEST(json_value_move)
{
	json::Value source{ parse_ok("[1,2,3]") };
	json::Value moved{ std::move(source) };

	CHECK(moved.is_array());
	CHECK_EQ(moved.size(), 3u);
	CHECK(source.is_null());

	json::Value target;
	target = std::move(moved);

	CHECK(target.is_array());
	CHECK_EQ(target.size(), 3u);
	CHECK(moved.is_null());
}

TEST(json_value_mutation)
{
	json::Value value{ json::Value::object() };
	value["name"] = "wcore";
	value["version"] = 1;
	value["tags"] = json::Value::array();
	value["tags"].push_back("core");
	value["tags"].push_back("foundation");

	CHECK_EQ(value["name"].as_string(), std::string("wcore"));
	CHECK_EQ(value.size(), 3u);

	json::Value& tags{ value["tags"] };
	REQUIRE(tags.is_array());
	CHECK_EQ(tags.size(), 2u);
	CHECK_EQ(tags[0].as_string(), std::string("core"));

	// operator[] on an object creates missing keys as null.
	CHECK(!value.contains("new"));
	CHECK(value["new"].is_null());
	CHECK(value.contains("new"));
}

TEST(json_value_integer_and_string_sizes)
{
	CHECK_EQ(json::Value(std::string("abc")).size(), 3u);
	CHECK_EQ(json::Value(7).size(), 0u);
	CHECK_EQ(json::Value::array().size(), 0u);
	CHECK(json::Value::object().empty());
}
