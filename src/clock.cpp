#include <wcore/clock.hpp>

namespace wcore
{
	Clock::Clock()
	{
		reset();
	}

	double Clock::tick()
	{
		const TimePoint now{ std::chrono::steady_clock::now() };
		mDelta = std::chrono::duration<double>(now - mLast).count();
		mLast = now;
		++mFrame;
		return mDelta;
	}

	double Clock::delta() const
	{
		return mDelta;
	}

	double Clock::elapsed() const
	{
		return std::chrono::duration<double>(mLast - mStart).count();
	}

	double Clock::fps() const
	{
		return mDelta > 0.0 ? 1.0 / mDelta : 0.0;
	}

	u64 Clock::frame() const
	{
		return mFrame;
	}

	void Clock::reset()
	{
		mStart = mLast = std::chrono::steady_clock::now();
		mDelta = 0.0;
		mFrame = 0;
	}
}
