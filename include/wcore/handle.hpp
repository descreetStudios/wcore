#pragma once

#include <cstddef>
#include <format>
#include <functional>
#include <utility>
#include <vector>

#include <wcore/assert.hpp>
#include <wcore/types.hpp>

namespace wcore
{
	template <typename T>
	class Handle
	{
	public:
		static constexpr u32 InvalidIndex{ ~u32{0} };

		constexpr Handle() = default;
		constexpr Handle(u32 index, u32 generation) noexcept
			: mIndex(index), mGeneration(generation) {}

		static constexpr Handle invalid() noexcept { return {}; }

		constexpr bool valid() const noexcept { return mIndex != InvalidIndex; }
		explicit constexpr operator bool() const noexcept { return valid(); }

		constexpr u32 index() const noexcept { return mIndex; }
		constexpr u32 generation() const noexcept { return mGeneration; }

		constexpr u64 raw() const noexcept
		{
			return (static_cast<u64>(mIndex) << 32) | mGeneration;
		}

		friend constexpr bool operator==(Handle a, Handle b) noexcept
		{
			return a.mIndex == b.mIndex && a.mGeneration == b.mGeneration;
		}

		friend constexpr bool operator!=(Handle a, Handle b) noexcept
		{
			return !(a == b);
		}

	private:
		u32 mIndex{ InvalidIndex };
		u32 mGeneration{ 0 };
	};

	template <typename T>
	class HandlePool
	{
	private:
		struct Slot
		{
			u32 generation{ 0 };
			u32 dense{ Handle<T>::InvalidIndex };
		};

		struct Entry
		{
			template <typename... Args>
			explicit Entry(u32 slot, Args&&... args)
				: value(std::forward<Args>(args)...), slot(slot) {}

			T value;
			u32 slot;
		};

	public:
		using HandleType = Handle<T>;

		HandlePool() = default;
		explicit HandlePool(std::size_t capacity) { reserve(capacity); }

		template <typename... Args>
		HandleType create(Args&&... args)
		{
			const u32 dense{ static_cast<u32>(mDense.size()) };
			u32 index{ 0 };

			if (!mFree.empty())
			{
				index = mFree.back();
				mDense.emplace_back(index, std::forward<Args>(args)...);
				mFree.pop_back();
			}
			else
			{
				WCORE_ASSERT(mSlots.size() < HandleType::InvalidIndex,
					"HandlePool cannot allocate another slot");

				index = static_cast<u32>(mSlots.size());
				mSlots.push_back({});

				try
				{
					mDense.emplace_back(index, std::forward<Args>(args)...);
				}
				catch (...)
				{
					mSlots.pop_back();
					throw;
				}
			}

			mSlots[index].dense = dense;
			return HandleType{ index, mSlots[index].generation };
		}

		bool destroy(HandleType handle)
		{
			Slot* slot{ find(handle) };
			if (!slot) return false;

			const u32 dense{ slot->dense };
			const u32 last{ static_cast<u32>(mDense.size()) - 1 };
			if (dense != last)
			{
				mDense[dense] = std::move(mDense[last]);
				mSlots[mDense[dense].slot].dense = dense;
			}

			mDense.pop_back();
			slot->dense = HandleType::InvalidIndex;
			++slot->generation;
			mFree.push_back(handle.index());
			return true;
		}

		T* get(HandleType handle)
		{
			const Slot* slot{ find(handle) };
			return slot ? &mDense[slot->dense].value : nullptr;
		}

		const T* get(HandleType handle) const
		{
			const Slot* slot{ find(handle) };
			return slot ? &mDense[slot->dense].value : nullptr;
		}

		bool alive(HandleType handle) const { return find(handle) != nullptr; }

		T& operator[](HandleType handle)
		{
			const Slot* slot{ find(handle) };
			WCORE_ASSERT_DEBUG(slot != nullptr,
				"HandlePool access with a stale or invalid handle");
			return mDense[slot->dense].value;
		}

		const T& operator[](HandleType handle) const
		{
			const Slot* slot{ find(handle) };
			WCORE_ASSERT_DEBUG(slot != nullptr,
				"HandlePool access with a stale or invalid handle");
			return mDense[slot->dense].value;
		}

		std::size_t size() const { return mDense.size(); }
		bool empty() const { return mDense.empty(); }
		std::size_t capacity() const { return mSlots.capacity(); }

		void reserve(std::size_t capacity)
		{
			mSlots.reserve(capacity);
			mDense.reserve(capacity);
			mFree.reserve(capacity);
		}

		void clear()
		{
			mDense.clear();
			mFree.clear();
			mFree.reserve(mSlots.size());

			for (std::size_t i{ 0 }; i < mSlots.size(); ++i)
			{
				Slot& slot{ mSlots[i] };
				if (slot.dense != HandleType::InvalidIndex)
				{
					slot.dense = HandleType::InvalidIndex;
					++slot.generation;
				}
				mFree.push_back(static_cast<u32>(i));
			}
		}

		template <typename Fn>
		void for_each(Fn&& fn)
		{
			for (Entry& entry : mDense)
				std::invoke(fn, entry.value);
		}

		template <typename Fn>
		void for_each(Fn&& fn) const
		{
			for (const Entry& entry : mDense)
				std::invoke(fn, entry.value);
		}

		template <typename Fn>
		void for_each_with_handle(Fn&& fn)
		{
			for (Entry& entry : mDense)
				std::invoke(fn, handle_for(entry), entry.value);
		}

		template <typename Fn>
		void for_each_with_handle(Fn&& fn) const
		{
			for (const Entry& entry : mDense)
				std::invoke(fn, handle_for(entry), entry.value);
		}

	private:
		Slot* find(HandleType handle)
		{
			if (handle.index() >= mSlots.size()) return nullptr;

			Slot& slot{ mSlots[handle.index()] };
			if (slot.dense == HandleType::InvalidIndex ||
				slot.generation != handle.generation())
			{
				return nullptr;
			}
			return &slot;
		}

		const Slot* find(HandleType handle) const
		{
			if (handle.index() >= mSlots.size()) return nullptr;

			const Slot& slot{ mSlots[handle.index()] };
			if (slot.dense == HandleType::InvalidIndex ||
				slot.generation != handle.generation())
			{
				return nullptr;
			}
			return &slot;
		}

		HandleType handle_for(const Entry& entry) const
		{
			return HandleType{ entry.slot, mSlots[entry.slot].generation };
		}

		std::vector<Slot> mSlots;
		std::vector<Entry> mDense;
		std::vector<u32> mFree;
	};
}

template <typename T>
struct std::formatter<wcore::Handle<T>>
{
	constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

	auto format(const wcore::Handle<T>& handle, std::format_context& ctx) const
	{
		if (!handle.valid())
			return std::format_to(ctx.out(), "Handle(invalid)");

		return std::format_to(ctx.out(), "Handle({}:{})",
			handle.index(), handle.generation());
	}
};

template <typename T>
struct std::hash<wcore::Handle<T>>
{
	std::size_t operator()(const wcore::Handle<T>& handle) const noexcept
	{
		u64 x{ handle.raw() };
		x ^= x >> 33;
		x *= 0xff51afd7ed558ccdULL;
		x ^= x >> 33;
		return static_cast<std::size_t>(x);
	}
};
