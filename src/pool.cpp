#include <wcore/pool.hpp>

#include <algorithm>
#include <limits>
#include <new>

#include <wcore/assert.hpp>

namespace wcore
{
	namespace
	{
#if defined(__STDCPP_DEFAULT_NEW_ALIGNMENT__)
		constexpr std::size_t DefaultNewAlignment{
			__STDCPP_DEFAULT_NEW_ALIGNMENT__ };
#else
		constexpr std::size_t DefaultNewAlignment{
			alignof(std::max_align_t) };
#endif
	}

	PoolAllocator::PoolAllocator(std::size_t blockSize,
		std::size_t alignment, std::size_t blocksPerSlab)
		: mBlocksPerSlab(blocksPerSlab == 0 ? 1 : blocksPerSlab)
	{
		WCORE_ASSERT(blockSize != 0, "Pool block size must be greater than zero");
		WCORE_ASSERT(alignment != 0 && (alignment & (alignment - 1)) == 0,
			"Pool alignment must be a power of two");

		mBlockAlignment = std::max(alignment, alignof(void*));
		mBlockSize = align_up(std::max(blockSize, sizeof(void*)),
			mBlockAlignment);

		WCORE_ASSERT(mBlockSize <=
			std::numeric_limits<std::size_t>::max() / mBlocksPerSlab,
			"Pool slab size overflow");
	}

	PoolAllocator::~PoolAllocator()
	{
		release();
	}

	PoolAllocator::PoolAllocator(PoolAllocator&& other) noexcept
		: mBlockSize(other.mBlockSize),
		mBlockAlignment(other.mBlockAlignment),
		mBlocksPerSlab(other.mBlocksPerSlab),
		mSlabs(std::move(other.mSlabs)),
		mFree(other.mFree),
		mUsed(other.mUsed),
		mCapacity(other.mCapacity)
	{
		other.mSlabs.clear();
		other.mFree = nullptr;
		other.mUsed = 0;
		other.mCapacity = 0;
	}

	PoolAllocator& PoolAllocator::operator=(PoolAllocator&& other) noexcept
	{
		if (this != &other)
		{
			release();

			mBlockSize = other.mBlockSize;
			mBlockAlignment = other.mBlockAlignment;
			mBlocksPerSlab = other.mBlocksPerSlab;
			mSlabs = std::move(other.mSlabs);
			mFree = other.mFree;
			mUsed = other.mUsed;
			mCapacity = other.mCapacity;

			other.mSlabs.clear();
			other.mFree = nullptr;
			other.mUsed = 0;
			other.mCapacity = 0;
		}
		return *this;
	}

	void* PoolAllocator::allocate()
	{
		if (!mFree) grow();

		FreeNode* node{ mFree };
		mFree = node->next;
		++mUsed;
		return node;
	}

	void PoolAllocator::deallocate(void* block) noexcept
	{
		if (!block) return;

		WCORE_ASSERT(mUsed > 0, "PoolAllocator deallocate underflow");
		mFree = ::new (block) FreeNode{ mFree };
		--mUsed;
	}

	void PoolAllocator::reset() noexcept
	{
		mFree = nullptr;

		for (void* data : mSlabs)
		{
			auto* block = static_cast<std::byte*>(data);
			for (std::size_t i{ 0 }; i < mBlocksPerSlab; ++i)
			{
				mFree = ::new (block) FreeNode{ mFree };
				block += mBlockSize;
			}
		}

		mUsed = 0;
	}

	void PoolAllocator::release() noexcept
	{
		for (void* data : mSlabs)
			free_raw(data, mBlockAlignment);

		mSlabs.clear();
		mFree = nullptr;
		mUsed = 0;
		mCapacity = 0;
	}

	void PoolAllocator::reserve(std::size_t blockCount)
	{
		while (mCapacity < blockCount) grow();
	}

	std::size_t PoolAllocator::align_up(std::size_t value, std::size_t alignment)
	{
		WCORE_ASSERT(
			value <= std::numeric_limits<std::size_t>::max() - (alignment - 1),
			"Pool block size overflow");
		return (value + alignment - 1) & ~(alignment - 1);
	}

	void* PoolAllocator::allocate_raw(std::size_t bytes, std::size_t alignment)
	{
		if (alignment > DefaultNewAlignment)
			return ::operator new(bytes, std::align_val_t(alignment));

		return ::operator new(bytes);
	}

	void PoolAllocator::free_raw(void* data, std::size_t alignment) noexcept
	{
		if (alignment > DefaultNewAlignment)
			::operator delete(data, std::align_val_t(alignment));
		else
			::operator delete(data);
	}

	void PoolAllocator::grow()
	{
		const std::size_t bytes{ mBlockSize * mBlocksPerSlab };
		void* data{ allocate_raw(bytes, mBlockAlignment) };

		try
		{
			mSlabs.push_back(data);
		}
		catch (...)
		{
			free_raw(data, mBlockAlignment);
			throw;
		}

		mCapacity += mBlocksPerSlab;

		auto* block = static_cast<std::byte*>(data);
		for (std::size_t i{ 0 }; i < mBlocksPerSlab; ++i)
		{
			mFree = ::new (block) FreeNode{ mFree };
			block += mBlockSize;
		}
	}
}