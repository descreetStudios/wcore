#include <wcore/test.hpp>
#include <wcore/clock.hpp>

#include <chrono>
#include <cmath>
#include <thread>

using namespace wcore;
using namespace test;

TEST(clock_starts_clean)
{
	Clock c;
	CHECK_EQ(c.frame(), 0);
	CHECK_EQ(c.delta(), 0.0);
	CHECK_EQ(c.elapsed(), 0.0);
	CHECK_EQ(c.fps(), 0.0);
}

TEST(clock_tick_advances_frame_and_time)
{
	Clock c;
	std::this_thread::sleep_for(std::chrono::milliseconds(5));

	const double d{ c.tick() };
	CHECK_EQ(c.frame(), 1);
	CHECK(d > 0.0);
	CHECK_EQ(c.delta(), d);
	CHECK(c.elapsed() >= d);
	CHECK(c.fps() > 0.0);
	CHECK(std::abs(c.fps() - 1.0 / d) < 1e-9);
}

TEST(clock_elapsed_only_advances_on_tick)
{
	Clock c;
	std::this_thread::sleep_for(std::chrono::milliseconds(2));
	CHECK_EQ(c.elapsed(), 0.0);

	c.tick();
	CHECK(c.elapsed() > 0.0);
}

TEST(clock_accumulates_frames)
{
	Clock c;
	std::this_thread::sleep_for(std::chrono::milliseconds(2));
	const double first{ c.tick() };

	std::this_thread::sleep_for(std::chrono::milliseconds(2));
	const double second{ c.tick() };

	CHECK_EQ(c.frame(), 2);
	CHECK(first > 0.0);
	CHECK(second > 0.0);
	CHECK(std::abs(c.elapsed() - (first + second)) < 1e-9);
}

TEST(clock_reset_clears_state)
{
	Clock c;
	c.tick();
	std::this_thread::sleep_for(std::chrono::milliseconds(2));
	c.reset();

	CHECK_EQ(c.frame(), 0);
	CHECK_EQ(c.delta(), 0.0);
	CHECK_EQ(c.elapsed(), 0.0);
	CHECK_EQ(c.fps(), 0.0);

	c.tick();
	CHECK_EQ(c.frame(), 1);
}
