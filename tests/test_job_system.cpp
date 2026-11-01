#include <wcore/test.hpp>
#include <wcore/job_system.hpp>

#include <atomic>
#include <cstddef>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace wcore;
using namespace test;

TEST(job_system_default_construction)
{
	JobSystem jobs;

	CHECK(jobs.worker_count() >= 1u);
	CHECK_EQ(jobs.worker_count(), JobSystem::default_worker_count());
	CHECK(jobs.idle());
	CHECK_EQ(jobs.pending(), 0u);
}

TEST(job_system_custom_worker_count)
{
	JobSystem jobs{ 3 };

	CHECK_EQ(jobs.worker_count(), 3u);
	CHECK(jobs.idle());
	CHECK_EQ(jobs.pending(), 0u);
}

TEST(job_system_submit_runs_job)
{
	JobSystem jobs{ 2 };
	std::atomic<int> value{ 0 };

	JobHandle handle{ jobs.submit([&] {
		value.store(42, std::memory_order_relaxed);
	}) };

	CHECK(handle.valid());
	handle.wait();

	CHECK_EQ(value.load(), 42);
	CHECK(handle.done());
	CHECK(jobs.idle());
	CHECK_EQ(jobs.pending(), 0u);
}

TEST(job_system_invalid_handle_is_safe)
{
	JobHandle handle;

	CHECK(!handle.valid());
	CHECK(handle.done());
	handle.wait();
}

TEST(job_system_handle_is_copyable)
{
	JobSystem jobs{ 2 };
	std::atomic<u32> counter{ 0 };

	JobHandle handle{ jobs.submit([&] {
		counter.fetch_add(1, std::memory_order_relaxed);
	}) };
	JobHandle copy{ handle };

	copy.wait();

	CHECK_EQ(counter.load(), 1u);
	CHECK(handle.done());
	CHECK(copy.done());
}

TEST(job_system_runs_many_jobs)
{
	JobSystem jobs{ 4 };
	constexpr u32 count{ 256 };
	std::atomic<u32> counter{ 0 };

	std::vector<JobHandle> handles;
	handles.reserve(count);

	for (u32 i{ 0 }; i < count; ++i)
	{
		handles.push_back(jobs.submit([&] {
			counter.fetch_add(1, std::memory_order_relaxed);
		}));
	}

	for (const JobHandle& handle : handles)
		handle.wait();

	CHECK_EQ(counter.load(), count);
	CHECK(jobs.idle());
}

TEST(job_system_wait_idle)
{
	JobSystem jobs{ 4 };
	constexpr u32 count{ 128 };
	std::atomic<u32> counter{ 0 };

	for (u32 i{ 0 }; i < count; ++i)
	{
		(void)jobs.submit([&] {
			counter.fetch_add(1, std::memory_order_relaxed);
		});
	}

	jobs.wait_idle();

	CHECK_EQ(counter.load(), count);
	CHECK(jobs.idle());
	CHECK_EQ(jobs.pending(), 0u);
}

TEST(job_system_nested_jobs_wait_idle)
{
	JobSystem jobs{ 2 };
	constexpr u32 parents{ 32 };
	constexpr u32 children{ 8 };
	std::atomic<u32> counter{ 0 };

	for (u32 i{ 0 }; i < parents; ++i)
	{
		(void)jobs.submit([&] {
			for (u32 j{ 0 }; j < children; ++j)
			{
				(void)jobs.submit([&] {
					counter.fetch_add(1, std::memory_order_relaxed);
				});
			}
		});
	}

	jobs.wait_idle();

	CHECK_EQ(counter.load(), parents * children);
	CHECK(jobs.idle());
}

TEST(job_system_parallel_for_covers_every_index)
{
	JobSystem jobs{ 4 };
	constexpr u32 count{ 1000 };
	std::atomic<u32> sum{ 0 };
	std::atomic<u32> calls{ 0 };

	jobs.parallel_for(count, [&](u32 index) {
		sum.fetch_add(index, std::memory_order_relaxed);
		calls.fetch_add(1, std::memory_order_relaxed);
	});

	CHECK_EQ(calls.load(), count);
	CHECK_EQ(sum.load(), count * (count - 1) / 2);
	CHECK(jobs.idle());
}

TEST(job_system_parallel_for_range)
{
	JobSystem jobs{ 2 };
	std::atomic<u32> sum{ 0 };
	std::atomic<u32> calls{ 0 };

	jobs.parallel_for(10u, 20u, [&](u32 index) {
		sum.fetch_add(index, std::memory_order_relaxed);
		calls.fetch_add(1, std::memory_order_relaxed);
	});

	CHECK_EQ(calls.load(), 10u);
	CHECK_EQ(sum.load(), 145u);
}

TEST(job_system_parallel_for_empty_range)
{
	JobSystem jobs{ 2 };
	std::atomic<u32> calls{ 0 };

	jobs.parallel_for(0u, 0u, [&](u32) {
		calls.fetch_add(1, std::memory_order_relaxed);
	});
	jobs.parallel_for(5u, 5u, [&](u32) {
		calls.fetch_add(1, std::memory_order_relaxed);
	});

	CHECK_EQ(calls.load(), 0u);
	CHECK(jobs.idle());
}

TEST(job_system_nested_submit_and_wait)
{
	JobSystem jobs{ 1 };
	std::atomic<int> value{ 0 };

	JobHandle outer{ jobs.submit([&] {
		JobHandle inner{ jobs.submit([&] {
			value.store(40, std::memory_order_relaxed);
		}) };
		inner.wait();
		value.fetch_add(2, std::memory_order_relaxed);
	}) };

	outer.wait();

	CHECK_EQ(value.load(), 42);
	CHECK(jobs.idle());
}

TEST(job_system_parallel_for_from_job)
{
	JobSystem jobs{ 1 };
	constexpr u32 count{ 500 };
	std::atomic<u32> sum{ 0 };

	JobHandle outer{ jobs.submit([&] {
		jobs.parallel_for(count, [&](u32 index) {
			sum.fetch_add(index, std::memory_order_relaxed);
		});
	}) };

	outer.wait();

	CHECK_EQ(sum.load(), count * (count - 1) / 2);
	CHECK(jobs.idle());
}

TEST(job_system_propagates_job_exception)
{
	JobSystem jobs{ 2 };

	JobHandle handle{ jobs.submit([] {
		throw std::runtime_error("job failed");
	}) };

	bool threw{ false };
	try
	{
		handle.wait();
	}
	catch (const std::runtime_error&)
	{
		threw = true;
	}

	CHECK(threw);
	CHECK(handle.done());
	CHECK(jobs.idle());
}

TEST(job_system_propagates_parallel_for_exception)
{
	JobSystem jobs{ 4 };
	std::atomic<u32> calls{ 0 };

	bool threw{ false };
	try
	{
		jobs.parallel_for(100u, [&](u32 index) {
			calls.fetch_add(1, std::memory_order_relaxed);
			if (index == 50u)
				throw std::runtime_error("chunk failed");
		});
	}
	catch (const std::runtime_error&)
	{
		threw = true;
	}

	CHECK(threw);
	CHECK(calls.load() <= 100u);
	CHECK(jobs.idle());
}

TEST(job_system_destructor_drains_jobs)
{
	constexpr u32 count{ 100 };
	std::atomic<u32> counter{ 0 };

	{
		JobSystem jobs{ 2 };

		for (u32 i{ 0 }; i < count; ++i)
		{
			(void)jobs.submit([&] {
				counter.fetch_add(1, std::memory_order_relaxed);
			});
		}
	}

	CHECK_EQ(counter.load(), count);
}

TEST(job_system_handle_outlives_system)
{
	std::atomic<int> value{ 0 };
	JobHandle handle;

	{
		JobSystem jobs{ 1 };
		handle = jobs.submit([&] {
			value.store(5, std::memory_order_relaxed);
		});
	}

	CHECK(handle.done());
	handle.wait();
	CHECK_EQ(value.load(), 5);
}

TEST(job_system_pending_and_idle)
{
	JobSystem jobs{ 1 };
	std::atomic<bool> release{ false };

	JobHandle handle{ jobs.submit([&] {
		while (!release.load(std::memory_order_acquire))
			std::this_thread::yield();
	}) };

	CHECK_EQ(jobs.pending(), 1u);
	CHECK(!jobs.idle());

	release.store(true, std::memory_order_release);
	handle.wait();

	CHECK(jobs.idle());
	CHECK_EQ(jobs.pending(), 0u);
}

TEST(job_system_jobs_run_off_main_thread)
{
	JobSystem jobs{ 1 };
	const std::thread::id mainId{ std::this_thread::get_id() };
	std::atomic<bool> onMain{ true };

	jobs.submit([&] {
		onMain.store(std::this_thread::get_id() == mainId,
			std::memory_order_relaxed);
	}).wait();

	CHECK(!onMain.load());
}

TEST(job_system_concurrent_submit)
{
	JobSystem jobs{ 4 };
	constexpr u32 producers{ 4 };
	constexpr u32 perProducer{ 64 };
	std::atomic<u32> counter{ 0 };

	std::vector<std::thread> threads;
	threads.reserve(producers);

	for (u32 i{ 0 }; i < producers; ++i)
	{
		threads.emplace_back([&] {
			for (u32 j{ 0 }; j < perProducer; ++j)
			{
				(void)jobs.submit([&] {
					counter.fetch_add(1, std::memory_order_relaxed);
				});
			}
		});
	}

	for (std::thread& thread : threads)
		thread.join();

	jobs.wait_idle();

	CHECK_EQ(counter.load(), producers * perProducer);
	CHECK(jobs.idle());
}
