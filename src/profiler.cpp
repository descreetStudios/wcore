#include <wcore/profiler.hpp>

#include <format>
#include <fstream>
#include <functional>
#include <thread>
#include <unordered_map>
#include <utility>

namespace wcore
{
	namespace
	{
		struct Frame
		{
			u64 session{ 0 };
			i64 startNs{ 0 };
			u32 depth{ 0 };
			std::string name;
			std::string category;
		};

		std::vector<Frame>& frames_for(u64 profilerId)
		{
			thread_local std::unordered_map<u64, std::vector<Frame>> stacks;
			return stacks[profilerId];
		}

		i64 now_ns()
		{
			using namespace std::chrono;
			return duration_cast<nanoseconds>(
				steady_clock::now().time_since_epoch()).count();
		}

		u64 next_profiler_id()
		{
			static std::atomic<u64> next{ 0 };
			return next.fetch_add(1, std::memory_order_relaxed) + 1;
		}

		u32 current_thread_id()
		{
			static thread_local const u32 id{
				static_cast<u32>(std::hash<std::thread::id>{}(
					std::this_thread::get_id()))
			};
			return id;
		}

		void append_json_string(std::string& out, std::string_view value)
		{
			out.push_back('"');
			for (const unsigned char c : value)
			{
				switch (c)
				{
				case '"':  out += "\\\""; break;
				case '\\': out += "\\\\"; break;
				case '\b': out += "\\b";  break;
				case '\f': out += "\\f";  break;
				case '\n': out += "\\n";  break;
				case '\r': out += "\\r";  break;
				case '\t': out += "\\t";  break;
				default:
					if (c < 0x20)
					{
						out += std::format("\\u{:04x}",
							static_cast<unsigned>(c));
					}
					else out.push_back(static_cast<char>(c));
					break;
				}
			}
			out.push_back('"');
		}
	} // anonymous namespace

	Profiler::Profiler()
		: mId(next_profiler_id())
		, mStartNs(now_ns())
	{
	}

	void Profiler::begin()
	{
		{
			std::lock_guard lock(mMutex);
			mEntries.clear();
		}

		mStartNs.store(now_ns(), std::memory_order_relaxed);
		mSession.fetch_add(1, std::memory_order_relaxed);
		mActive.store(true, std::memory_order_release);
	}

	void Profiler::end()
	{
		mActive.store(false, std::memory_order_release);
	}

	void Profiler::clear()
	{
		std::lock_guard lock(mMutex);
		mEntries.clear();
	}

	bool Profiler::active() const
	{
		return mActive.load(std::memory_order_acquire);
	}

	double Profiler::elapsed_ms() const
	{
		const i64 elapsed{ now_ns() - mStartNs.load(std::memory_order_relaxed) };
		return static_cast<double>(elapsed) * 1e-6;
	}

	Profiler::ScopeToken Profiler::begin_scope(std::string_view name,
		std::string_view category)
	{
		if (!mActive.load(std::memory_order_acquire)) return 0;

		const u64 session{ mSession.load(std::memory_order_relaxed) };
		std::vector<Frame>& frames{ frames_for(mId) };

		// Drop frames left behind by an earlier session.
		if (!frames.empty() && frames.back().session != session)
			frames.clear();

		Frame frame;
		frame.session = session;
		frame.startNs = now_ns();
		frame.depth = static_cast<u32>(frames.size());
		frame.name.assign(name);
		frame.category.assign(category);
		frames.push_back(std::move(frame));

		return session;
	}

	void Profiler::end_scope()
	{
		std::vector<Frame>& frames{ frames_for(mId) };
		if (frames.empty()) return;

		end_scope(frames.back().session);
	}

	void Profiler::end_scope(ScopeToken token)
	{
		if (token == 0) return;

		std::vector<Frame>& frames{ frames_for(mId) };
		if (frames.empty() || frames.back().session != token) return;

		Frame frame{ std::move(frames.back()) };
		frames.pop_back();

		// The session may have been stopped or restarted while the scope was
		// open, in which case the record is stale and must be dropped.
		if (!mActive.load(std::memory_order_acquire)) return;
		if (frame.session != mSession.load(std::memory_order_relaxed)) return;

		ProfileEntry entry;
		entry.name = std::move(frame.name);
		entry.category = std::move(frame.category);
		entry.startMs = static_cast<double>(
			frame.startNs - mStartNs.load(std::memory_order_relaxed)) * 1e-6;
		entry.durationMs = static_cast<double>(now_ns() - frame.startNs) * 1e-6;
		entry.threadId = current_thread_id();
		entry.depth = frame.depth;

		std::lock_guard lock(mMutex);
		mEntries.push_back(std::move(entry));
	}

	std::vector<ProfileEntry> Profiler::entries() const
	{
		std::lock_guard lock(mMutex);
		return mEntries;
	}

	u64 Profiler::size() const
	{
		std::lock_guard lock(mMutex);
		return static_cast<u64>(mEntries.size());
	}

	std::string Profiler::to_chrome_trace() const
	{
		const std::vector<ProfileEntry> snapshot{ entries() };

		std::string out;
		out.reserve(64 + snapshot.size() * 128);
		out += R"({"displayTimeUnit":"ms","traceEvents":[)";

		bool first{ true };
		for (const ProfileEntry& entry : snapshot)
		{
			if (!first) out.push_back(',');
			first = false;

			out += R"({"name":)";
			append_json_string(out, entry.name);
			out += R"(,"cat":)";
			append_json_string(out, entry.category);
			out += std::format(
				R"(,"ph":"X","ts":{:.3f},"dur":{:.3f},"pid":1,"tid":{})",
				entry.startMs * 1000.0,
				entry.durationMs * 1000.0,
				entry.threadId);
			out.push_back('}');
		}

		out += "]}";
		return out;
	}

	bool Profiler::write_chrome_trace(std::string_view filePath) const
	{
		std::ofstream out{ std::string(filePath),
			std::ios::binary | std::ios::trunc };
		if (!out) return false;

		out << to_chrome_trace();
		return out.good();
	}

	Profiler& Profiler::instance()
	{
		static Profiler profiler;
		return profiler;
	}

	Profiler::Scope::Scope(std::string_view name,
		std::string_view category, Profiler& profiler)
		: mProfiler(&profiler)
		, mToken(profiler.begin_scope(name, category))
	{
	}

	Profiler::Scope::~Scope()
	{
		if (mToken != 0) mProfiler->end_scope(mToken);
	}

	Profiler::Scope::Scope(Scope&& other) noexcept
		: mProfiler(other.mProfiler)
		, mToken(other.mToken)
	{
		other.mProfiler = nullptr;
		other.mToken = 0;
	}

	Profiler::Scope& Profiler::Scope::operator=(Scope&& other) noexcept
	{
		if (this != &other)
		{
			if (mToken != 0 && mProfiler != nullptr)
				mProfiler->end_scope(mToken);

			mProfiler = other.mProfiler;
			mToken = other.mToken;
			other.mProfiler = nullptr;
			other.mToken = 0;
		}
		return *this;
	}
}
