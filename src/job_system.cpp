#include <wcore/job_system.hpp>

#include <exception>
#include <mutex>
#include <thread>
#include <utility>

#include <wcore/assert.hpp>

namespace wcore
{
	void JobHandle::wait() const
	{
		if (!mState) return;

		if (!mState->done.load(std::memory_order_acquire))
		{
			JobSystem* owner{ mState->owner.load(std::memory_order_acquire) };
			if (owner && owner->is_worker_thread())
				owner->help_until(*mState);
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

	JobSystem::JobSystem(u32 workerCount)
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

	JobSystem::~JobSystem()
	{
		shutdown();
	}

	u32 JobSystem::default_worker_count() noexcept
	{
		const u32 hardware{ std::thread::hardware_concurrency() };
		return hardware > 1 ? hardware - 1 : 1;
	}

	void JobSystem::wait_idle()
	{
		std::unique_lock<std::mutex> lock(mMutex);
		mCv.wait(lock, [this] { return mPending == 0; });
	}

	u32 JobSystem::pending() const
	{
		std::lock_guard<std::mutex> lock(mMutex);
		return mPending;
	}

	JobSystem*& JobSystem::tls_worker() noexcept
	{
		static thread_local JobSystem* worker{ nullptr };
		return worker;
	}

	void JobSystem::worker_loop()
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

	bool JobSystem::try_pop(Job& job)
	{
		std::lock_guard<std::mutex> lock(mMutex);
		if (mJobs.empty()) return false;

		job = std::move(mJobs.front());
		mJobs.pop_front();
		return true;
	}

	void JobSystem::run_job(Job& job)
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

	void JobSystem::help_until(const JobHandle::State& state)
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

	void JobSystem::shutdown() noexcept
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
}