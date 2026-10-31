#pragma once

#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <utility>

#include <wcore/types.hpp>

namespace wcore::logger
{
	enum class Level : u8
	{
		Trace, Debug, Info,
		Warn, Error, Fatal,
		Off
	};

	// Configures the console logger and, when filePath is not empty, a file
	// sink. The path is a filesystem path, so non-ASCII names work on every
	// platform. Throws when the file cannot be opened.
	void init(Level level = Level::Info,
		const std::filesystem::path& filePath = {});
	void set_level(Level level);

	// Pattern string, e.g. "[%H:%M:%S] [%^%l%$] %v"
	void set_pattern(std::string_view patter);

	void flush();

	namespace internal
	{
		void write(Level level, std::string_view msg);
	}

	template <typename... Args>
	void trace(std::format_string<Args...> fmt, Args&&... args)
	{
		internal::write(Level::Trace, std::format(fmt, std::forward<Args>(args)...));
	}

	template <typename... Args>
	void debug(std::format_string<Args...> fmt, Args&&... args)
	{
		internal::write(Level::Debug, std::format(fmt, std::forward<Args>(args)...));
	}

	template <typename... Args>
	void info(std::format_string<Args...> fmt, Args&&... args)
	{
		internal::write(Level::Info, std::format(fmt, std::forward<Args>(args)...));
	}

	template <typename... Args>
	void warn(std::format_string<Args...> fmt, Args&&... args)
	{
		internal::write(Level::Warn, std::format(fmt, std::forward<Args>(args)...));
	}

	template <typename... Args>
	void error(std::format_string<Args...> fmt, Args&&... args)
	{
		internal::write(Level::Error, std::format(fmt, std::forward<Args>(args)...));
	}

	template <typename... Args>
	void fatal(std::format_string<Args...> fmt, Args&&... args)
	{
		internal::write(Level::Fatal, std::format(fmt, std::forward<Args>(args)...));
	}
}
