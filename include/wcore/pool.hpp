#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>
#include <new>
#include <utility>
#include <vector>

#include <wcore/assert.hpp>

namespace wcore
{
	// A fixed-size block pool with an intrusive free list.
	class PoolAllocator
	{
	public:
		PoolAllocator(std::size_t blockSize,
			std::size_t alignment = alignof(std::max_align_t),
			std::size_t blocksPerSlab = 64)
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

		~PoolAllocator() { release(); }

		PoolAllocator(const PoolAllocator&) = delete;
		PoolAllocator& operator=(const PoolAllocator&) = delete;

		PoolAllocator(PoolAllocator&& other) noexcept
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

		PoolAllocator& operator=(PoolAllocator&& other) noexcept
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

		// Returns one block, growing the pool when the free list is empty.
		[[nodiscard]] void* allocate()
		{
			if (!mFree) grow();

			FreeNode* node{ mFree };
			mFree = node->next;
			++mUsed;
			return node;
		}

		// Returns block to the free list. Does not run destructors.
		void deallocate(void* block) noexcept
		{
			if (!block) return;

			WCORE_ASSERT(mUsed > 0, "PoolAllocator deallocate underflow");
			mFree = ::new (block) FreeNode{ mFree };
			--mUsed;
		}

		// Returns every block to the free list without freeing slabs.
		// Live objects are not destroyed.
		void reset() noexcept
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

		// Frees every slab. Live objects are not destroyed.
		void release() noexcept
		{
			for (void* data : mSlabs)
				free_raw(data, mBlockAlignment);

			mSlabs.clear();
			mFree = nullptr;
			mUsed = 0;
			mCapacity = 0;
		}

		// Preallocates enough slabs for at least blockCount blocks.
		void reserve(std::size_t blockCount)
		{
			while (mCapacity < blockCount) grow();
		}

		std::size_t block_size() const noexcept { return mBlockSize; }
		std::size_t block_alignment() const noexcept { return mBlockAlignment; }
		std::size_t blocks_per_slab() const noexcept { return mBlocksPerSlab; }
		std::size_t slab_count() const noexcept { return mSlabs.size(); }
		std::size_t size() const noexcept { return mUsed; }
		std::size_t capacity() const noexcept { return mCapacity; }
		std::size_t available() const noexcept { return mCapacity - mUsed; }
		bool empty() const noexcept { return mUsed == 0; }

	private:
		struct FreeNode
		{
			FreeNode* next;
		};

#if defined(__STDCPP_DEFAULT_NEW_ALIGNMENT__)
		static constexpr std::size_t DefaultNewAlignment{
			__STDCPP_DEFAULT_NEW_ALIGNMENT__ };
#else
		static constexpr std::size_t DefaultNewAlignment{
			alignof(std::max_align_t) };
#endif

		static std::size_t align_up(std::size_t value, std::size_t alignment)
		{
			WCORE_ASSERT(
				value <= std::numeric_limits<std::size_t>::max() - (alignment - 1),
				"Pool block size overflow");
			return (value + alignment - 1) & ~(alignment - 1);
		}

		static void* allocate_raw(std::size_t bytes, std::size_t alignment)
		{
			if (alignment > DefaultNewAlignment)
				return ::operator new(bytes, std::align_val_t(alignment));

			return ::operator new(bytes);
		}

		static void free_raw(void* data, std::size_t alignment) noexcept
		{
			if (alignment > DefaultNewAlignment)
				::operator delete(data, std::align_val_t(alignment));
			else
				::operator delete(data);
		}

		void grow()
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

		std::size_t mBlockSize{ 0 };
		std::size_t mBlockAlignment{ 0 };
		std::size_t mBlocksPerSlab{ 1 };
		std::vector<void*> mSlabs;
		FreeNode* mFree{ nullptr };
		std::size_t mUsed{ 0 };
		std::size_t mCapacity{ 0 };
	};

	// A typed object pool built on PoolAllocator.
	template <typename T>
	class Pool
	{
	public:
		explicit Pool(std::size_t blocksPerSlab = 64)
			: mAllocator(sizeof(T), alignof(T), blocksPerSlab) {}

		Pool(const Pool&) = delete;
		Pool& operator=(const Pool&) = delete;

		Pool(Pool&&) noexcept = default;
		Pool& operator=(Pool&&) noexcept = default;

		// Returns an unconstructed block.
		[[nodiscard]] T* allocate()
		{
			return static_cast<T*>(mAllocator.allocate());
		}

		// Returns a block without running T's destructor.
		void deallocate(T* object) noexcept
		{
			mAllocator.deallocate(object);
		}

		// Constructs a T in a pooled block. The block is returned to the pool
		// if the constructor throws.
		template <typename... Args>
		[[nodiscard]] T* create(Args&&... args)
		{
			T* object{ allocate() };

			try
			{
				return ::new (static_cast<void*>(object))
					T(std::forward<Args>(args)...);
			}
			catch (...)
			{
				deallocate(object);
				throw;
			}
		}

		// Runs T's destructor and returns the block to the pool.
		void destroy(T* object)
		{
			if (!object) return;

			object->~T();
			deallocate(object);
		}

		void reset() noexcept { mAllocator.reset(); }
		void release() noexcept { mAllocator.release(); }
		void reserve(std::size_t blockCount) { mAllocator.reserve(blockCount); }

		std::size_t block_size() const noexcept { return mAllocator.block_size(); }
		std::size_t block_alignment() const noexcept
		{
			return mAllocator.block_alignment();
		}
		std::size_t blocks_per_slab() const noexcept
		{
			return mAllocator.blocks_per_slab();
		}
		std::size_t slab_count() const noexcept { return mAllocator.slab_count(); }
		std::size_t size() const noexcept { return mAllocator.size(); }
		std::size_t capacity() const noexcept { return mAllocator.capacity(); }
		std::size_t available() const noexcept { return mAllocator.available(); }
		bool empty() const noexcept { return mAllocator.empty(); }

	private:
		PoolAllocator mAllocator;
	};
} // namespace wcore
