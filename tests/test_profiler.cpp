#include <wcore/test.hpp>
#include <wcore/profiler.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

using namespace wcore;
using namespace test;

namespace fs = std::filesystem;

static fs::path temp_trace(const char* name)
{
	return fs::temp_directory_path() /
		(std::string("wcore_test_") + name + ".json");
}

static std::string read_file(const fs::path& path)
{
	std::ifstream in(path, std::ios::binary);
	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

static bool contains(const std::string& haystack, const std::string& needle)
{
	return haystack.find(needle) != std::string::npos;
}

TEST(profiler_starts_inactive_with_no_entries)
{
	Profiler p;
	CHECK(!p.active());
	CHECK_EQ(p.size(), 0);
	CHECK(p.entries().empty());

	CHECK_EQ(p.begin_scope("ignored"), 0);
	p.end_scope();
	CHECK_EQ(p.size(), 0);
}

TEST(profiler_begin_and_end_control_capture)
{
	Profiler p;
	p.begin();
	CHECK(p.active());

	p.end();
	CHECK(!p.active());

	{
		Profiler::Scope s{ "after", "", p };
	}
	CHECK_EQ(p.size(), 0);
}

TEST(profiler_records_scoped_duration)
{
	Profiler p;
	p.begin();
	{
		Profiler::Scope s{ "work", "test", p };
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	p.end();

	const std::vector<ProfileEntry> entries{ p.entries() };
	REQUIRE(entries.size() == 1);
	CHECK_EQ(entries[0].name, "work");
	CHECK_EQ(entries[0].category, "test");
	CHECK(entries[0].durationMs > 0.0);
	CHECK(entries[0].startMs >= 0.0);
	CHECK_EQ(entries[0].depth, 0);
}

TEST(profiler_records_nesting_depth)
{
	Profiler p;
	p.begin();
	{
		Profiler::Scope outer{ "outer", "", p };
		{
			Profiler::Scope inner{ "inner", "", p };
		}
	}
	p.end();

	const std::vector<ProfileEntry> entries{ p.entries() };
	REQUIRE(entries.size() == 2);
	CHECK_EQ(entries[0].name, "inner");
	CHECK_EQ(entries[0].depth, 1);
	CHECK_EQ(entries[1].name, "outer");
	CHECK_EQ(entries[1].depth, 0);
	CHECK(entries[1].durationMs >= entries[0].durationMs);
}

TEST(profiler_records_manual_scopes)
{
	Profiler p;
	p.begin();

	const Profiler::ScopeToken token{ p.begin_scope("manual", "cat") };
	CHECK(token != 0);
	p.end_scope();
	p.end();

	const std::vector<ProfileEntry> entries{ p.entries() };
	REQUIRE(entries.size() == 1);
	CHECK_EQ(entries[0].name, "manual");
	CHECK_EQ(entries[0].category, "cat");
}

TEST(profiler_macro_records_function_name)
{
	Profiler& p{ Profiler::instance() };
	p.begin();
	{
		WCORE_PROFILE_FUNCTION();
	}
	p.end();

	const std::vector<ProfileEntry> entries{ p.entries() };
	REQUIRE(entries.size() == 1);
	CHECK_EQ(entries[0].name, std::string("profiler_macro_records_function_name"));
}

TEST(profiler_macro_records_category)
{
	Profiler& p{ Profiler::instance() };
	p.begin();
	{
		WCORE_PROFILE_SCOPE_CATEGORY("categorized", "render");
	}
	p.end();

	const std::vector<ProfileEntry> entries{ p.entries() };
	REQUIRE(entries.size() == 1);
	CHECK_EQ(entries[0].name, "categorized");
	CHECK_EQ(entries[0].category, "render");
}

TEST(profiler_records_multiple_threads)
{
	Profiler p;
	p.begin();

	{
		Profiler::Scope main{ "main", "", p };
		std::thread worker{ [&p]() {
			Profiler::Scope scope{ "worker", "", p };
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		} };
		worker.join();
	}
	p.end();

	const std::vector<ProfileEntry> entries{ p.entries() };
	REQUIRE(entries.size() == 2);
	CHECK_EQ(entries[0].name, "worker");
	CHECK_EQ(entries[1].name, "main");
	CHECK(entries[0].threadId != entries[1].threadId);
}

TEST(profiler_begin_starts_new_session)
{
	Profiler p;
	p.begin();
	{
		Profiler::Scope s{ "first", "", p };
	}

	p.begin();
	{
		Profiler::Scope s{ "second", "", p };
	}
	p.end();

	const std::vector<ProfileEntry> entries{ p.entries() };
	REQUIRE(entries.size() == 1);
	CHECK_EQ(entries[0].name, "second");
}

TEST(profiler_clear_discards_entries)
{
	Profiler p;
	p.begin();
	{
		Profiler::Scope s{ "one", "", p };
	}
	REQUIRE(p.size() == 1);

	p.clear();
	CHECK_EQ(p.size(), 0);
	p.end();
}

TEST(profiler_elapsed_tracks_session)
{
	Profiler p;
	p.begin();

	const double first{ p.elapsed_ms() };
	std::this_thread::sleep_for(std::chrono::milliseconds(2));
	const double second{ p.elapsed_ms() };

	CHECK(second > first);
	p.end();
}

TEST(profiler_chrome_trace_contains_events)
{
	Profiler p;
	p.begin();
	{
		Profiler::Scope s{ "draw", "render", p };
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	p.end();

	const std::string trace{ p.to_chrome_trace() };
	CHECK(trace.front() == '{');
	CHECK(trace.back() == '}');
	CHECK(contains(trace, "\"displayTimeUnit\":\"ms\""));
	CHECK(contains(trace, "\"traceEvents\""));
	CHECK(contains(trace, "\"name\":\"draw\""));
	CHECK(contains(trace, "\"cat\":\"render\""));
	CHECK(contains(trace, "\"ph\":\"X\""));
	CHECK(contains(trace, "\"pid\":1"));
}

TEST(profiler_chrome_trace_escapes_json_strings)
{
	Profiler p;
	p.begin();
	{
		Profiler::Scope s{ "quote\"slash\\newline\n", "cat\t", p };
	}
	p.end();

	const std::string trace{ p.to_chrome_trace() };
	CHECK(contains(trace, "quote\\\"slash\\\\newline\\n"));
	CHECK(contains(trace, "cat\\t"));
}

TEST(profiler_writes_chrome_trace_file)
{
	const fs::path path{ temp_trace("chrome") };
	std::error_code ec;
	fs::remove(path, ec);

	Profiler p;
	p.begin();
	{
		Profiler::Scope s{ "file_scope", "", p };
	}
	p.end();

	CHECK(p.write_chrome_trace(path.string()));
	CHECK(fs::exists(path));
	CHECK(contains(read_file(path), "file_scope"));

	fs::remove(path, ec);
}

TEST(profiler_write_trace_fails_for_invalid_path)
{
	Profiler p;
	p.begin();
	{
		Profiler::Scope s{ "scope", "", p };
	}
	p.end();

	CHECK(!p.write_chrome_trace(""));
}

TEST(profiler_writes_chrome_trace_to_non_ascii_path)
{
	const fs::path path{ fs::temp_directory_path() /
		fs::path(u8"wcore_test_tr\u00E4ce_\u00FC.json") };

	std::error_code ec;
	fs::remove(path, ec);

	Profiler p;
	p.begin();
	{
		Profiler::Scope s{ "unicode_scope", "", p };
	}
	p.end();

	CHECK(p.write_chrome_trace(path));
	CHECK(contains(read_file(path), "unicode_scope"));

	fs::remove(path, ec);
	CHECK(!fs::exists(path));
}
