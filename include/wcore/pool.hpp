#pragma once

#include <cstddef>
#include <new>
#include <utility>
#include <vector>

namespace wcore
{
	class PoolAllocator
	{
	public:
		PoolAllocator(std::size_t blockSize,
			std::size_t alignment = alignof(std::max_align_t),
			std::size_t blocksPerSlab = 64);

		~PoolAllocator();

		PoolAllocator(const PoolAllocator&) = delete;
		PoolAllocator& operator=(const PoolAllocator&) = delete;

		PoolAllocator(PoolAllocator&& other) noexcept;
		PoolAllocator& operator=(PoolAllocator&& other) noexcept;

		[[nodiscard]] void* allocate();
		void deallocate(void* block) noexcept;
		void reset() noexcept;
		void release() noexcept;
		void reserve(std::size_t blockCount);

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

		static std::size_t align_up(std::size_t value, std::size_t alignment);
		static void* allocate_raw(std::size_t bytes, std::size_t alignment);
		static void free_raw(void* data, std::size_t alignment) noexcept;
		void grow();

		std::size_t mBlockSize{ 0 };
		std::size_t mBlockAlignment{ 0 };
		std::size_t mBlocksPerSlab{ 1 };
		std::vector<void*> mSlabs;
		FreeNode* mFree{ nullptr };
		std::size_t mUsed{ 0 };
		std::size_t mCapacity{ 0 };
	};

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

		[[nodiscard]] T* allocate()
		{
			return static_cast<T*>(mAllocator.allocate());
		}

		void deallocate(T* object) noexcept
		{
			mAllocator.deallocate(object);
		}

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
}
