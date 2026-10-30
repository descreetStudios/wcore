#pragma once

#include <chrono>

#include <wcore/types.hpp>

namespace wcore
{
	// Frame timing based on a steady clock.
	class Clock
	{
	public:
		using TimePoint = std::chrono::steady_clock::time_point;

		Clock();

		// Advances one frame and returns the delta in seconds.
		double tick();

		// Seconds between the previous two ticks (0 before the first tick).
		double delta() const;

		// Seconds from construction/reset() to the last tick.
		double elapsed() const;

		// Frames per second based on the last delta (0 before the first tick).
		double fps() const;

		// Frames ticked since construction or reset().
		u64 frame() const;

		// Restarts timing and clears the frame counter.
		void reset();

	private:
		TimePoint mStart;
		TimePoint mLast;
		double mDelta;
		u64 mFrame;
	};
}
