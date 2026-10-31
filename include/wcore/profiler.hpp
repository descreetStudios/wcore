#pragma once

#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <wcore/types.hpp>

namespace wcore
{
	// A single completed profiling scope.
	struct ProfileEntry
	{
		std::string name;
		std::string category;
		double startMs{ 0.0 };    // Start relative to begin() of the session.
		double durationMs{ 0.0 };
		u32 threadId{ 0 };
		u32 depth{ 0 };           // Nesting depth on the recording thread.
	};

	// Lightweight CPU profiler.
	class Profiler
	{
	public:
		using ScopeToken = u64;

		static Profiler& instance();

		// RAII scope timer. Records on sdestruction when the profiler is active.
		class Scope
		{
		public:
			explicit Scope(std::string_view name,
				std::string_view category = "",
				Profiler& profiler = Profiler::instance());
			~Scope();

			Scope(const Scope&) = delete;
			Scope& operator=(const Scope&) = delete;
			Scope(Scope&& other) noexcept;
			Scope& operator=(Scope&& other) noexcept;

		private:
			Profiler* mProfiler{ nullptr };
			ScopeToken mToken{ 0 };
		};

		Profiler();
		~Profiler() = default;

		Profiler(const Profiler&) = delete;
		Profiler& operator=(const Profiler&) = delete;
		Profiler(Profiler&&) = delete;
		Profiler& operator=(Profiler&&) = delete;

		// Starts a new session, discarding any previously captured records.
		void begin();

		// Stops recording. Already captured records are kept.
		void end();

		// Discards all captured records without changing the session state.
		void clear();

		bool active() const;

		// Milliseconds since the last begin().
		double elapsed_ms() const;

		// Opens a scope on the calling thread and returns a token used to close
		// it. Returns 0 when no session is active.
		ScopeToken begin_scope(std::string_view name,
			std::string_view category = "");

		// Closes the most recent scope opened on the calling thread.
		void end_scope();

		// Closes the scope associated with token. Ignored when it is not the
		// most recent scope on the calling thread.
		void end_scope(ScopeToken token);

		// Snapshot of all completed scopes, in completion order.
		std::vector<ProfileEntry> entries() const;
		u64 size() const;

		// Serializes the captured scopes as Chrome Trace JSON.
		std::string to_chrome_trace() const;

		// Writes to_chrome_trace() to disk. Returns false when the file cannot
		// be opened or written. The path is a filesystem path, so non-ASCII
		// names work on every platform.
		bool write_chrome_trace(const std::filesystem::path& filePath) const;

	private:
		const u64 mId;
		mutable std::mutex mMutex;
		std::vector<ProfileEntry> mEntries;
		std::atomic<bool> mActive{ false };
		std::atomic<u64> mSession{ 0 };
		std::atomic<i64> mStartNs{ 0 };
	};
} // namespace wcore

#define WCORE_DETAIL_PROFILE_CONCAT_IMPL(a, b) a##b
#define WCORE_DETAIL_PROFILE_CONCAT(a, b) WCORE_DETAIL_PROFILE_CONCAT_IMPL(a, b)

#ifdef WCORE_PROFILE_DISABLED
#define WCORE_PROFILE_SCOPE(name) ((void)0)
#define WCORE_PROFILE_SCOPE_CATEGORY(name, category) ((void)0)
#define WCORE_PROFILE_FUNCTION() ((void)0)
#define WCORE_PROFILE_FUNCTION_CATEGORY(category) ((void)0)
#else
#define WCORE_PROFILE_SCOPE(name) \
	::wcore::Profiler::Scope WCORE_DETAIL_PROFILE_CONCAT(wcore_profile_scope_, __COUNTER__){ (name) }

#define WCORE_PROFILE_SCOPE_CATEGORY(name, category) \
	::wcore::Profiler::Scope WCORE_DETAIL_PROFILE_CONCAT(wcore_profile_scope_, __COUNTER__){ (name), (category) }

#define WCORE_PROFILE_FUNCTION() WCORE_PROFILE_SCOPE(__func__)
#define WCORE_PROFILE_FUNCTION_CATEGORY(category) WCORE_PROFILE_SCOPE_CATEGORY(__func__, (category))
#endif
