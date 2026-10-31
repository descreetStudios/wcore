#include <wcore/assert.hpp>
#include <wcore/logger.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/dist_sink.h>
#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <string>

namespace wcore::logger
{
    // Declared before the logger so the stream outlives it during static
    // destruction.
    static std::unique_ptr<std::ofstream> gFileStream;
    static std::shared_ptr<spdlog::logger> gLogger;

    static spdlog::level::level_enum to_spdlog(Level level)
    {
        switch (level)
        {
        case Level::Trace: return spdlog::level::trace;
        case Level::Debug: return spdlog::level::debug;
        case Level::Info: return spdlog::level::info;
        case Level::Warn: return spdlog::level::warn;
        case Level::Error: return spdlog::level::err;
        case Level::Fatal: return spdlog::level::critical;
        case Level::Off: return spdlog::level::off;
        }
        WCORE_UNREACHABLE("unknown wcore::logger::Level");
    }

    void init(Level level, const std::filesystem::path& filePath)
    {
        // Drop the previous logger first: its sink still writes to the stream.
        gLogger.reset();
        gFileStream.reset();

        auto distSink{ std::make_shared<spdlog::sinks::dist_sink_mt>() };

        auto consoleSink{ std::make_shared<spdlog::sinks::stdout_color_sink_mt>() };
        distSink->add_sink(consoleSink);

        if (!filePath.empty())
        {
            // The stream is opened from a filesystem::path, so non-ASCII names
            // work on Windows too (spdlog's own file sinks only take narrow
            // names unless SPDLOG_WCHAR_FILENAMES is set).
            auto stream{ std::make_unique<std::ofstream>(
                filePath, std::ios::binary | std::ios::trunc) };

            if (!stream->is_open())
            {
                throw spdlog::spdlog_ex(std::format(
                    "failed opening log file {}", filePath.string()));
            }

            auto fileSink{
                std::make_shared<spdlog::sinks::ostream_sink_mt>(*stream) };
            distSink->add_sink(fileSink);
            gFileStream = std::move(stream);
        }

        gLogger = std::make_shared<spdlog::logger>("desaf", distSink);
        gLogger->set_level(to_spdlog(level));
        gLogger->flush_on(spdlog::level::warn);

        set_pattern("[%H:%M:%S] [%^%l%$] %v");
    }

    void set_level(Level level)
    {
        WCORE_ASSERT(gLogger != nullptr,
            "wcore::logger::init() must be called before set_level()");
        gLogger->set_level(to_spdlog(level));
    }

    void set_pattern(std::string_view pattern)
    {
        WCORE_ASSERT(gLogger != nullptr,
            "wcore::logger::init() must be called before set_pattern()");
        gLogger->set_pattern(std::string(pattern));
    }

    void flush()
    {
        WCORE_ASSERT(gLogger != nullptr,
            "wcore::logger::init() must be called before flush()");
        gLogger->flush();
    }

    namespace internal
    {
        void write(Level level, std::string_view msg)
        {
            WCORE_ASSERT(gLogger != nullptr,
                "wcore::logger::init() must be called before logging");
            gLogger->log(to_spdlog(level), "{}", msg);
        }
    }
}
