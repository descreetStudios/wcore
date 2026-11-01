#include <wcore/arena.hpp>

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include <wcore/assert.hpp>

namespace wcore
{
	void Arena::Scope::end()
	{
		if (mArena)
		{
			mArena->restore(mMarker);
			mArena = nullptr;
		}
	}

	Arena::Scope::Scope(Scope&& other) noexcept
		: mArena(other.mArena), mMarker(other.mMarker)
	{
		other.mArena = nullptr;
	}

	Arena::Scope& Arena::Scope::operator=(Scope&& other) noexcept
	{
		if (this != &other)
		{
			end();
			mArena = other.mArena;
			mMarker = other.mMarker;
			other.mArena = nullptr;
		}
		return *this;
	}

	Arena::Arena(Arena&& other) noexcept
		: mBlocks(std::move(other.mBlocks)),
		mBlockSize(other.mBlockSize),
		mCurrent(other.mCurrent)
	{
		other.mBlocks.clear();
		other.mCurrent = 0;
	}

	Arena& Arena::operator=(Arena&& other) noexcept
	{
		if (this != &other)
		{
			mBlocks = std::move(other.mBlocks);
			mBlockSize = other.mBlockSize;
			mCurrent = other.mCurrent;
			other.mBlocks.clear();
			other.mCurrent = 0;
		}
		return *this;
	}

	void* Arena::allocate(std::size_t size, std::size_t alignment)
	{
		if (size == 0) return nullptr;

		WCORE_ASSERT(alignment != 0 && (alignment & (alignment - 1)) == 0,
			"Arena alignment must be a power of two");
		WCORE_ASSERT(
			size <= std::numeric_limits<std::size_t>::max() - (alignment - 1),
			"Arena allocation size overflow");

		const std::size_t required{ size + alignment - 1 };

		if (mBlocks.empty())
			append_block(std::max(mBlockSize, required));

		if (void* memory{ try_allocate(*mBlocks[mCurrent], size, alignment) })
			return memory;

		void* memory{ try_allocate(*acquire_block(required), size, alignment) };
		WCORE_ASSERT(memory != nullptr, "Arena block is too small for allocation");
		return memory;
	}

	void Arena::reset()
	{
		for (auto& block : mBlocks)
			block->used = 0;
		mCurrent = 0;
	}

	void Arena::release()
	{
		mBlocks.clear();
		mCurrent = 0;
	}

	Arena::Marker Arena::mark() const
	{
		if (mBlocks.empty()) return Marker{};
		return Marker{ mCurrent, mBlocks[mCurrent]->used };
	}

	void Arena::restore(Marker marker)
	{
		if (mBlocks.empty())
		{
			WCORE_ASSERT(marker.block == 0 && marker.used == 0,
				"Arena marker is invalid");
			return;
		}

		WCORE_ASSERT(marker.block < mBlocks.size(), "Arena marker is invalid");
		WCORE_ASSERT(marker.used <= mBlocks[marker.block]->capacity,
			"Arena marker is invalid");

		for (std::size_t i{ marker.block + 1 }; i < mBlocks.size(); ++i)
			mBlocks[i]->used = 0;

		mBlocks[marker.block]->used = marker.used;
		mCurrent = marker.block;
	}

	std::size_t Arena::capacity() const
	{
		std::size_t total{ 0 };
		for (const auto& block : mBlocks)
			total += block->capacity;
		return total;
	}

	std::size_t Arena::used() const
	{
		std::size_t total{ 0 };
		for (const auto& block : mBlocks)
			total += block->used;
		return total;
	}

	void* Arena::try_allocate(Block& block, std::size_t size,
		std::size_t alignment)
	{
		void* pointer{ block.data + block.used };
		std::size_t space{ block.capacity - block.used };

		void* aligned{ std::align(alignment, size, pointer, space) };
		if (!aligned) return nullptr;

		const auto offset = static_cast<std::size_t>(
			static_cast<std::byte*>(aligned) - block.data);
		block.used = offset + size;
		return aligned;
	}

	void Arena::append_block(std::size_t capacity)
	{
		mBlocks.push_back(std::make_unique<Block>(capacity));
		mCurrent = mBlocks.size() - 1;
	}

	Arena::Block* Arena::acquire_block(std::size_t required)
	{
		for (std::size_t i{ mCurrent + 1 }; i < mBlocks.size(); ++i)
		{
			if (mBlocks[i]->capacity >= required)
			{
				mCurrent = i;
				mBlocks[i]->used = 0;
				return mBlocks[i].get();
			}
		}

		append_block(std::max(mBlockSize, required));
		return mBlocks.back().get();
	}
}