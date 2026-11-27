#include <wcore/log.hpp>

#include <catch2/catch_test_macros.hpp>

#include <spdlog/details/os.h>
#include <spdlog/sinks/ostream_sink.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>

namespace
{
	struct DefaultLoggerGuard
	{
		std::shared_ptr<spdlog::logger> previous{ spdlog::default_logger() };

		~DefaultLoggerGuard()
		{
			spdlog::set_default_logger(previous);
		}
	};

	std::shared_ptr<spdlog::logger> make_test_logger(std::ostream& output)
	{
		auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(output);
		auto logger = std::make_shared<spdlog::logger>("wcore-test", sink);
		logger->set_pattern("%v");
		logger->set_level(spdlog::level::trace);
		return logger;
	}
} // namespace

TEST_CASE("log macros write through the default logger", "[log]")
{
	std::ostringstream output;
	DefaultLoggerGuard guard;
	spdlog::set_default_logger(make_test_logger(output));

	WCORE_INFO("hello {}", 42);

	REQUIRE(output.str() == std::string{ "hello 42" } + SPDLOG_EOL);
}

TEST_CASE("log level filters messages", "[log]")
{
	std::ostringstream output;
	DefaultLoggerGuard guard;
	spdlog::set_default_logger(make_test_logger(output));

	wcore::log::set_level(wcore::log::Level::Warn);
	REQUIRE(wcore::log::level() == wcore::log::Level::Warn);

	WCORE_INFO("hidden");
	WCORE_WARN("visible");

	REQUIRE(output.str() == std::string{ "visible" } + SPDLOG_EOL);
}

TEST_CASE("init configures a file logger", "[log]")
{
	const auto path = std::filesystem::temp_directory_path() / "wcore-log-test.log";
	std::error_code error;
	std::filesystem::remove(path, error);

	wcore::log::Config config;
	config.console = false;
	config.file = path.string();
	config.pattern = "%v";

	{
		DefaultLoggerGuard guard;

		REQUIRE(wcore::log::init(config));
		WCORE_INFO("written to file");
		wcore::log::flush();
	}

	std::ifstream input(path, std::ios::binary);
	std::stringstream contents;
	contents << input.rdbuf();

	REQUIRE(contents.str() == std::string{ "written to file" } + SPDLOG_EOL);

	std::filesystem::remove(path, error);
}
