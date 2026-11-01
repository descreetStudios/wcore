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
	struct ProfileEntry
	{
		std::string name;
		std::string category;
		double startMs{ 0.0 };
		double durationMs{ 0.0 };
		u32 threadId{ 0 };
		u32 depth{ 0 };
	};

	class Profiler
	{
	public:
		using ScopeToken = u64;

		static Profiler& instance();

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

		void begin();
		void end();
		void clear();

		bool active() const;
		double elapsed_ms() const;

		ScopeToken begin_scope(std::string_view name,
			std::string_view category = "");
		void end_scope();
		void end_scope(ScopeToken token);

		std::vector<ProfileEntry> entries() const;
		u64 size() const;

		std::string to_chrome_trace() const;

		bool write_chrome_trace(const std::filesystem::path& filePath) const;

	private:
		const u64 mId;
		mutable std::mutex mMutex;
		std::vector<ProfileEntry> mEntries;
		std::atomic<bool> mActive{ false };
		std::atomic<u64> mSession{ 0 };
		std::atomic<i64> mStartNs{ 0 };
	};
}

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
