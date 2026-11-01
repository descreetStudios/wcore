#include <wcore/test.hpp>
#include <wcore/logger.hpp>
#include <wcore/math.hpp>

#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <string>
#include <type_traits>

static_assert(std::is_same_v<wcore::vec3, glm::vec3>);
static_assert(std::is_same_v<wcore::mat4, glm::mat4>);
static_assert(std::is_same_v<wcore::quat, glm::quat>);

using namespace wcore;
using namespace test;

namespace fs = std::filesystem;

static fs::path temp_log(const char* name)
{
	return fs::temp_directory_path() /
		(std::string("wcore_test_") + name + ".log");
}

static std::string read_file(const fs::path& p)
{
	logger::flush();
	std::ifstream in(p, std::ios::binary);
	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

static bool contains(const std::string& haystack, const char* needle)
{
	return haystack.find(needle) != std::string::npos;
}

struct TempFile
{
	fs::path path;
	explicit TempFile(const char* name) : path(temp_log(name))
	{
		std::error_code ec;
		fs::remove(path, ec);
	}
	~TempFile()
	{
		logger::init(logger::Level::Off);
		std::error_code ec;
		fs::remove(path, ec);
	}
	std::string str() const { return path.string(); }
};

TEST(math_adl_functions_unqualified)
{
	const vec3 v{ 1.0f, 2.0f, 2.0f };
	const vec3 n{ normalize(v) };
	CHECK(std::abs(length(n) - 1.0f) < 1e-6f);

	const vec3 ex{ 1.0f, 0.0f, 0.0f };
	const vec3 ey{ 0.0f, 1.0f, 0.0f };
	const vec3 ez{ cross(ex, ey) };
	CHECK(std::abs(ez.z - 1.0f) < 1e-6f);
	CHECK(std::abs(dot(ex, ez)) < 1e-6f);
}

TEST(math_scalar_whitelist)
{
	constexpr float tolerance{ 1e-6f };
	CHECK(std::abs(radians(90.0f) - pi<float>() / 2.0f) < tolerance);
	CHECK(std::abs(degrees(half_pi<float>()) - 90.0f) < tolerance);
	CHECK(std::abs(two_pi<float>() - 2.0f * pi<float>()) < tolerance);

	const mat4 p{ perspective(radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f) };
	CHECK(p == glm::perspective(glm::radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f));

	const mat4 o{ ortho(-2.0f, 2.0f, -1.0f, 1.0f, 0.1f, 100.0f) };
	CHECK(o == glm::ortho(-2.0f, 2.0f, -1.0f, 1.0f, 0.1f, 100.0f));
}

TEST(math_formatter_vectors)
{
	CHECK(std::format("{}", vec2{ 1.0f, 2.0f }) == "(1, 2)");
	CHECK(std::format("{}", vec3{ 1.0f, 2.0f, 3.0f }) == "(1, 2, 3)");
	CHECK(std::format("{}", vec4{ 1.0f, 2.0f, 3.0f, 4.0f }) == "(1, 2, 3, 4)");
	CHECK(std::format("{}", ivec3{ 7, 8, 9 }) == "(7, 8, 9)");
	CHECK(std::format("{}", uvec2{ 1u, 2u }) == "(1, 2)");
	CHECK(std::format("{}", dvec2{ 1.0, 2.5 }) == "(1, 2.5)");
}

TEST(math_formatter_spec_applies_to_every_element)
{
	CHECK(std::format("{:.2f}", vec3{ 1.2345f, 2.5f, 3.0f }) == "(1.23, 2.50, 3.00)");
	CHECK(std::format("{:5.1f}", vec3{ 1.5f, 2.5f, 3.5f }) == "(  1.5,   2.5,   3.5)");
	CHECK(std::format("{:5d}", ivec3{ 1, 22, 333 }) == "(    1,    22,   333)");
}

TEST(math_formatter_mat_and_quat)
{
	CHECK(std::format("{}", mat2{ 1.0f }) == "[1, 0; 0, 1]");
	CHECK(std::format("{}", mat4{ 1.0f }) ==
		"[1, 0, 0, 0; 0, 1, 0, 0; 0, 0, 1, 0; 0, 0, 0, 1]");

	const quat q{ 1.0f, 2.0f, 3.0f, 4.0f };
	CHECK(std::format("{}", q) == "(1, 2, 3, 4)");
}

TEST(logger_formats_math_types)
{
	TempFile f("mathlog");
	logger::init(logger::Level::Info, f.str());

	logger::info("pos={} q={}", vec3{ 1.0f, 2.0f, 3.0f },
		quat{ 1.0f, 2.0f, 3.0f, 4.0f });

	const std::string out{ read_file(f.path) };
	CHECK(contains(out, "pos=(1, 2, 3)"));
	CHECK(contains(out, "q=(1, 2, 3, 4)"));
}