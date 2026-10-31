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

	// A copyable handle to a submitted job.
	class JobHandle
	{
	public:
		JobHandle() noexcept = default;

		JobHandle(const JobHandle&) noexcept = default;
		JobHandle& operator=(const JobHandle&) noexcept = default;
		JobHandle(JobHandle&&) noexcept = default;
		JobHandle& operator=(JobHandle&&) noexcept = default;
		~JobHandle() = default;

		// True when this handle refers to a submitted job.
		[[nodiscard]] bool valid() const noexcept { return mState != nullptr; }

		// True once the job has completed. An invalid handle counts as done.
		[[nodiscard]] bool done() const noexcept
		{
			return !mState || mState->done.load(std::memory_order_acquire);
		}

		// Waits for the job to finish and rethrows its exception, if any.
		// Calling wait() from a worker helps run queued jobs instead of
		// blocking, so a job can safely wait for work it submitted.
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

	// A fixed-size worker pool with a shared job queue.
	class JobSystem
	{
	public:
		// workerCount == 0 selects default_worker_count().
		explicit JobSystem(u32 workerCount = 0)
		{
			const u32 count{ workerCount == 0 ? default_worker_count()
				: workerCount };
			WCORE_ASSERT(count > 0, "JobSystem needs at least one worker");

			mWorkers.reserve(count);
			try
			{
				for (u32 i{ 0 }; i < count; ++i)
					mWorkers.emplace_back([this] { worker_loop(); });
			}
			catch (...)
			{
				shutdown();
				throw;
			}
		}

		~JobSystem() { shutdown(); }

		JobSystem(const JobSystem&) = delete;
		JobSystem& operator=(const JobSystem&) = delete;
		JobSystem(JobSystem&&) = delete;
		JobSystem& operator=(JobSystem&&) = delete;

		// Leaves one hardware thread for the caller when possible.
		[[nodiscard]] static u32 default_worker_count() noexcept
		{
			const u32 hardware{ std::thread::hardware_concurrency() };
			return hardware > 1 ? hardware - 1 : 1;
		}

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

		// Waits until every submitted job, including jobs submitted by other
		// jobs, has completed. Not valid from inside a job.
		void wait_idle()
		{
			std::unique_lock<std::mutex> lock(mMutex);
			mCv.wait(lock, [this] { return mPending == 0; });
		}

		// Calls fn(index) for every index in [0, count) and waits for all of
		// the work to finish. Exceptions are rethrown after all chunks finish.
		template <typename F>
		void parallel_for(u32 count, F&& fn)
		{
			parallel_for(0, count, std::forward<F>(fn));
		}

		// Calls fn(index) for every index in [begin, end) and waits for all of
		// the work to finish.
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

		// Number of submitted jobs that have not finished yet.
		[[nodiscard]] u32 pending() const
		{
			std::lock_guard<std::mutex> lock(mMutex);
			return mPending;
		}

		[[nodiscard]] bool idle() const { return pending() == 0; }

	private:
		friend class JobHandle;

		struct Job
		{
			std::function<void()> fn;
			JobHandle handle;
		};

		// Identifies the JobSystem whose worker is executing on this thread.
		static JobSystem*& tls_worker() noexcept
		{
			static thread_local JobSystem* worker{ nullptr };
			return worker;
		}

		[[nodiscard]] bool is_worker_thread() const noexcept
		{
			return tls_worker() == this;
		}

		void worker_loop()
		{
			tls_worker() = this;

			for (;;)
			{
				Job job;
				{
					std::unique_lock<std::mutex> lock(mMutex);
					mCv.wait(lock, [this] {
						return mStopping || !mJobs.empty();
					});

					if (mJobs.empty())
					{
						if (mStopping) break;
						continue;
					}

					job = std::move(mJobs.front());
					mJobs.pop_front();
				}

				run_job(job);
			}

			tls_worker() = nullptr;
		}

		bool try_pop(Job& job)
		{
			std::lock_guard<std::mutex> lock(mMutex);
			if (mJobs.empty()) return false;

			job = std::move(mJobs.front());
			mJobs.pop_front();
			return true;
		}

		void run_job(Job& job)
		{
			std::exception_ptr error;
			try
			{
				job.fn();
			}
			catch (...)
			{
				error = std::current_exception();
			}

			// Publish completion and drop the pending count under both
			// locks, so wait() and wait_idle() observe a consistent result.
			JobHandle::State& state{ *job.handle.mState };
			{
				std::lock_guard<std::mutex> systemLock(mMutex);
				std::lock_guard<std::mutex> stateLock(state.mutex);

				WCORE_ASSERT(mPending > 0,
					"JobSystem pending counter underflow");
				--mPending;

				state.error = error;
				state.done.store(true, std::memory_order_release);
				// The job no longer needs a live system to help it, so a
				// handle that outlives the JobSystem stays safe to wait on.
				state.owner.store(nullptr, std::memory_order_release);
			}
			mCv.notify_all();
			state.cv.notify_all();
		}

		void help_until(const JobHandle::State& state)
		{
			while (!state.done.load(std::memory_order_acquire))
			{
				Job job;
				if (try_pop(job))
					run_job(job);
				else
					std::this_thread::yield();
			}
		}

		void shutdown() noexcept
		{
			{
				std::lock_guard<std::mutex> lock(mMutex);
				if (mStopping) return;
				mStopping = true;
			}
			mCv.notify_all();

			for (std::thread& worker : mWorkers)
			{
				if (worker.joinable())
					worker.join();
			}
			mWorkers.clear();
		}

		mutable std::mutex mMutex;
		std::condition_variable mCv;
		std::deque<Job> mJobs;
		std::vector<std::thread> mWorkers;
		u32 mPending{ 0 };
		bool mStopping{ false };
	};

	inline void JobHandle::wait() const
	{
		if (!mState) return;

		if (!mState->done.load(std::memory_order_acquire))
		{
			JobSystem* owner{ mState->owner.load(std::memory_order_acquire) };
			if (owner && owner->is_worker_thread())
			{
				owner->help_until(*mState);
			}
			else
			{
				std::unique_lock<std::mutex> lock(mState->mutex);
				mState->cv.wait(lock, [this] {
					return mState->done.load(std::memory_order_acquire);
				});
			}
		}

		if (mState->error)
			std::rethrow_exception(mState->error);
	}
} // namespace wcore
