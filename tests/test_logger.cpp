#include <wcore/test.hpp>
#include <wcore/logger.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

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

TEST(logger_init_default_does_not_crash)
{
	logger::init();
	logger::info("default init {}", 1);
	CHECK(true);
}

TEST(logger_all_levels_callable_with_format_args)
{
	logger::init(logger::Level::Trace);
	logger::trace("trace {}", 1);
	logger::debug("debug {} {}", 2, "two");
	logger::info("info {:.2f}", 3.14159);
	logger::warn("warn {}", std::string("four"));
	logger::error("error {:#x}", 255);
	logger::fatal("fatal {}", 'x');
	logger::info("no args");
	CHECK(true);
}

TEST(logger_writes_formatted_message_to_file)
{
	TempFile f("format");
	logger::init(logger::Level::Info, f.str());
	logger::info("hello {} {}", "world", 42);

	const std::string out = read_file(f.path);
	CHECK(contains(out, "hello world 42"));
}

TEST(logger_file_receives_all_levels_when_trace)
{
	TempFile f("alllevels");
	logger::init(logger::Level::Trace, f.str());
	logger::trace("msg_trace");
	logger::debug("msg_debug");
	logger::info("msg_info");
	logger::warn("msg_warn");
	logger::error("msg_error");

	const std::string out{ read_file(f.path) };
	CHECK(contains(out, "msg_trace"));
	CHECK(contains(out, "msg_debug"));
	CHECK(contains(out, "msg_info"));
	CHECK(contains(out, "msg_warn"));
	CHECK(contains(out, "msg_error"));
}

TEST(logger_init_level_filters_lower_levels)
{
	TempFile f("initfilter");
	logger::init(logger::Level::Warn, f.str());
	logger::trace("skip_trace");
	logger::debug("skip_debug");
	logger::info("skip_info");
	logger::warn("keep_warn");
	logger::error("keep_error");

	const std::string out{ read_file(f.path) };
	CHECK(!contains(out, "skip_trace"));
	CHECK(!contains(out, "skip_debug"));
	CHECK(!contains(out, "skip_info"));
	CHECK(contains(out, "keep_warn"));
	CHECK(contains(out, "keep_error"));
}

TEST(logger_set_level_changes_filtering_at_runtime)
{
	TempFile f("setlevel");
	logger::init(logger::Level::Error, f.str());

	logger::info("before_info");
	logger::set_level(logger::Level::Info);
	logger::info("after_info");
	logger::debug("after_debug");

	const std::string out{ read_file(f.path) };
	CHECK(!contains(out, "before_info"));
	CHECK(contains(out, "after_info"));
	CHECK(!contains(out, "after_debug"));
}

TEST(logger_level_off_writes_nothing)
{
	TempFile f("off");
	logger::init(logger::Level::Off, f.str());
	logger::trace("off_trace");
	logger::error("off_error");
	logger::fatal("off_fatal");

	const std::string out{ read_file(f.path) };
	CHECK(!contains(out, "off_trace"));
	CHECK(!contains(out, "off_error"));
	CHECK(!contains(out, "off_fatal"));
}

TEST(logger_set_pattern_applies_to_output)
{
	TempFile f("pattern");
	logger::init(logger::Level::Info, f.str());
	logger::set_pattern("PREFIX|%v");
	logger::info("patterned");

	const std::string out{ read_file(f.path) };
	CHECK(contains(out, "PREFIX|patterned"));
}

TEST(logger_level_enum_ordering)
{
	using L = logger::Level;
	CHECK(L::Trace < L::Debug);
	CHECK(L::Debug < L::Info);
	CHECK(L::Info < L::Warn);
	CHECK(L::Warn < L::Error);
	CHECK(L::Error < L::Fatal);
	CHECK(L::Fatal < L::Off);
}

TEST(logger_writes_to_non_ascii_path)
{
	const fs::path path{ fs::temp_directory_path() /
		fs::path(u8"wcore_test_\u00FCn\u00EFc\u00F6d\u00E9.log") };

	std::error_code ec;
	fs::remove(path, ec);

	logger::init(logger::Level::Info, path);
	logger::info("unicode path {}", 7);
	logger::flush();

	std::string content;
	{
		std::ifstream in(path, std::ios::binary);
		std::ostringstream ss;
		ss << in.rdbuf();
		content = ss.str();
	}
	CHECK(contains(content, "unicode path 7"));

	logger::init(logger::Level::Off);
	fs::remove(path, ec);
	CHECK(!fs::exists(path));
}