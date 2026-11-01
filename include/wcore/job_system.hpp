#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <wcore/assert.hpp>
#include <wcore/types.hpp>

namespace wcore
{
	class JobSystem;

	class JobHandle
	{
	public:
		JobHandle() noexcept = default;

		JobHandle(const JobHandle&) noexcept = default;
		JobHandle& operator=(const JobHandle&) noexcept = default;
		JobHandle(JobHandle&&) noexcept = default;
		JobHandle& operator=(JobHandle&&) noexcept = default;
		~JobHandle() = default;

		[[nodiscard]] bool valid() const noexcept { return mState != nullptr; }
		[[nodiscard]] bool done() const noexcept
		{
			return !mState || mState->done.load(std::memory_order_acquire);
		}

		void wait() const;

	private:
		friend class JobSystem;

		struct State
		{
			std::atomic<bool> done{ false };
			std::atomic<JobSystem*> owner{ nullptr };
			std::exception_ptr error;

			std::mutex mutex;
			std::condition_variable cv;
		};

		explicit JobHandle(std::shared_ptr<State> state) noexcept
			: mState(std::move(state)) {}

		std::shared_ptr<State> mState;
	};

	class JobSystem
	{
	public:
		explicit JobSystem(u32 workerCount = 0);
		~JobSystem();

		JobSystem(const JobSystem&) = delete;
		JobSystem& operator=(const JobSystem&) = delete;
		JobSystem(JobSystem&&) = delete;
		JobSystem& operator=(JobSystem&&) = delete;

		// Leaves one hardware thread for the caller when possible.
		[[nodiscard]] static u32 default_worker_count() noexcept;

		[[nodiscard]] u32 worker_count() const noexcept
		{
			return static_cast<u32>(mWorkers.size());
		}

		// Enqueues a callable and returns a handle for it. The callable must
		// be copy-constructible, as it is stored in a std::function.
		template <typename F>
		[[nodiscard]] JobHandle submit(F&& fn)
		{
			static_assert(std::is_invocable_v<std::decay_t<F>&>,
				"Job must be callable with no arguments");

			auto state = std::make_shared<JobHandle::State>();
			state->owner.store(this, std::memory_order_relaxed);

			Job job;
			job.fn = std::function<void()>(std::forward<F>(fn));
			job.handle = JobHandle{ state };

			{
				std::lock_guard<std::mutex> lock(mMutex);
				WCORE_ASSERT(!mStopping, "JobSystem is shutting down");
				mJobs.push_back(std::move(job));
				++mPending;
			}

			mCv.notify_one();
			return JobHandle{ std::move(state) };
		}

		void wait_idle();

		template <typename F>
		void parallel_for(u32 count, F&& fn)
		{
			parallel_for(0, count, std::forward<F>(fn));
		}

		template <typename F>
		void parallel_for(u32 begin, u32 end, F&& fn)
		{
			if (begin >= end) return;

			const u32 total{ end - begin };
			const u64 requested{ static_cast<u64>(worker_count()) * 4u };
			const u32 jobCount{
				static_cast<u32>(std::min<u64>(total, requested)) };
			const u32 chunks{ std::max(1u, jobCount) };

			std::atomic<u32> next{ begin };
			auto run = [&next, end, &fn] {
				for (;;)
				{
					const u32 index{
						next.fetch_add(1, std::memory_order_relaxed) };
					if (index >= end) return;
					fn(index);
				}
			};

			std::vector<JobHandle> handles;
			handles.reserve(chunks);

			try
			{
				for (u32 i{ 0 }; i < chunks; ++i)
					handles.push_back(submit(run));
			}
			catch (...)
			{
				for (const JobHandle& handle : handles)
				{
					try { handle.wait(); }
					catch (...) {}
				}
				throw;
			}

			std::exception_ptr error;
			for (const JobHandle& handle : handles)
			{
				try
				{
					handle.wait();
				}
				catch (...)
				{
					if (!error) error = std::current_exception();
				}
			}

			if (error) std::rethrow_exception(error);
		}

		[[nodiscard]] u32 pending() const;

		[[nodiscard]] bool idle() const { return pending() == 0; }

	private:
		friend class JobHandle;

		struct Job
		{
			std::function<void()> fn;
			JobHandle handle;
		};

		static JobSystem*& tls_worker() noexcept;

		[[nodiscard]] bool is_worker_thread() const noexcept
		{
			return tls_worker() == this;
		}

		void worker_loop();
		bool try_pop(Job& job);
		void run_job(Job& job);
		void help_until(const JobHandle::State& state);
		void shutdown() noexcept;

		mutable std::mutex mMutex;
		std::condition_variable mCv;
		std::deque<Job> mJobs;
		std::vector<std::thread> mWorkers;
		u32 mPending{ 0 };
		bool mStopping{ false };
	};
}
