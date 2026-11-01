#pragma once

#include <chrono>

#include <wcore/types.hpp>

namespace wcore
{
	class Clock
	{
	public:
		using TimePoint = std::chrono::steady_clock::time_point;

		Clock();

		double tick();
		double delta() const;
		double elapsed() const;
		double fps() const;
		u64 frame() const;
		void reset();

	private:
		TimePoint mStart;
		TimePoint mLast;
		double mDelta;
		u64 mFrame;
	};
}
