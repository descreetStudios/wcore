#pragma once

#include <cstddef>
#include <string>

#include <spdlog/spdlog.h>

namespace wcore::log
{
	enum class Level
	{
		trace,
		debug,
		info,
		warn,
		error,
		critical,
		off,
	};

	struct Config
	{
		Level minLevel{ Level::info };
		bool console{ true };
		std::string file;
		bool rotate{ false };
		std::size_t maxFileSize{ 5 * 1024 * 1024 };
		std::size_t maxFiles{ 3 };
		bool truncate{ true };
		std::string pattern{ "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v" };
	};

	bool init(const Config& config = {});
	void shutdown() noexcept;

	void flush() noexcept;
	void set_level(Level level) noexcept;
	Level level() noexcept;

	struct LogSilencer
	{
		Level previous{ level() };

		LogSilencer()
		{
			set_level(Level::off);
		}

		~LogSilencer()
		{
			set_level(previous);
		}
	};
} // namespace wcore::log

#define WCORE_TRACE(...) SPDLOG_TRACE(__VA_ARGS__)
#define WCORE_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
#define WCORE_INFO(...) SPDLOG_INFO(__VA_ARGS__)
#define WCORE_WARN(...) SPDLOG_WARN(__VA_ARGS__)
#define WCORE_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)
#define WCORE_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__)
