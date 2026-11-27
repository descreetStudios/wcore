#include <wcore/log.hpp>

#include <cstdio>
#include <exception>
#include <memory>
#include <utility>
#include <vector>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace wcore::log
{
	namespace
	{
		spdlog::level::level_enum to_spdlog(Level level) noexcept
		{
			switch (level)
			{
			case Level::Trace: return spdlog::level::trace;
			case Level::Debug: return spdlog::level::debug;
			case Level::Info: return spdlog::level::info;
			case Level::Warn: return spdlog::level::warn;
			case Level::Error: return spdlog::level::err;
			case Level::Critical: return spdlog::level::critical;
			case Level::Off: return spdlog::level::off;
			default: return spdlog::level::info;
			}
		}

		Level from_spdlog(spdlog::level::level_enum level) noexcept
		{
			switch (level)
			{
			case spdlog::level::trace: return Level::Trace;
			case spdlog::level::debug: return Level::Debug;
			case spdlog::level::info: return Level::Info;
			case spdlog::level::warn: return Level::Warn;
			case spdlog::level::err: return Level::Error;
			case spdlog::level::critical: return Level::Critical;
			case spdlog::level::off: return Level::Off;
			default: return Level::Info;
			}
		}

		spdlog::sink_ptr make_file_sink(const Config& config)
		{
			if (config.rotate)
			{
				return std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
					config.file, config.maxFileSize, config.maxFiles);
			}

			return std::make_shared<spdlog::sinks::basic_file_sink_mt>(
				config.file, config.truncate);
		}
	} // namespace

	bool init(const Config& config)
	{
		bool ok{ true };
		std::vector<spdlog::sink_ptr> sinks;

		if (config.console)
		{
			try
			{
				sinks.push_back(std::make_shared<
					spdlog::sinks::stdout_color_sink_mt>());
			}
			catch (const std::exception& ex)
			{
				std::fprintf(stderr, "wcore: failed to create console log sink: %s\n", ex.what());
				ok = false;
			}
		}

		if (!config.file.empty())
		{
			try
			{
				sinks.push_back(make_file_sink(config));
			}
			catch (const std::exception& ex)
			{
				std::fprintf(stderr, "wcore: failed to create file log sink '%s': %s\n",
					config.file.c_str(), ex.what());
				ok = false;
			}
		}

		if (sinks.empty())
		{
			std::fprintf(stderr, "wcore: logging requires at least one working sink\n");
			return false;
		}

		auto logger = std::make_shared<spdlog::logger>(
			"wcore", sinks.begin(), sinks.end());

		logger->set_level(to_spdlog(config.minLevel));
		if (!config.pattern.empty())
			logger->set_pattern(config.pattern);

		logger->flush_on(spdlog::level::err);
		logger->set_error_handler([](const std::string& message)
		{
			std::fprintf(stderr, "wcore: logging error: %s\n", message.c_str());
		});

		spdlog::set_default_logger(std::move(logger));
		return ok;
	}

	void shutdown() noexcept
	{
		try
		{
			if (auto logger = spdlog::default_logger())
				logger->flush();

			spdlog::shutdown();
		}
		catch (...) {}
	}

	void flush() noexcept
	{
		try
		{
			if (auto logger = spdlog::default_logger())
				logger->flush();
		}
		catch (...) {}
	}

	void set_level(Level level) noexcept
	{
		if (auto logger = spdlog::default_logger())
			logger->set_level(to_spdlog(level));
	}

	Level level() noexcept
	{
		if (auto logger = spdlog::default_logger())
			return from_spdlog(logger->level());

		return Level::Off;
	}
} // namespace wcore::log
