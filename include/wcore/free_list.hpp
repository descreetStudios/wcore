#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <utility>

#include <wcore/assert.hpp>

namespace wcore
{
	// A variable-size free-list allocator.
	class FreeListAllocator
	{
	private:
		struct alignas(std::max_align_t) Block
		{
			std::size_t size{ 0 };       // Total block size, including this header.
			std::size_t prevSize{ 0 };   // Size of the previous physical block.
			Block* nextFree{ nullptr };
			Block* prevFree{ nullptr };
			bool free{ false };
		};

		static constexpr std::size_t HeaderSize{ sizeof(Block) };
		static constexpr std::size_t HeaderAlignment{ alignof(Block) };
		static constexpr std::size_t MinBlockSize{ sizeof(Block) };

#if defined(__STDCPP_DEFAULT_NEW_ALIGNMENT__)
		static constexpr std::size_t DefaultNewAlignment{
			__STDCPP_DEFAULT_NEW_ALIGNMENT__ };
#else
		static constexpr std::size_t DefaultNewAlignment{
			alignof(std::max_align_t) };
#endif

	public:
		// Allocates and owns a backing buffer of capacity bytes.
		explicit FreeListAllocator(std::size_t capacity)
		{
			if (capacity == 0) return;

			mOwnsMemory = true;
			mBase = allocate_raw(capacity, HeaderAlignment);
			set_memory(mBase, capacity);
		}

		// Uses caller-owned memory without taking ownership.
		FreeListAllocator(void* memory, std::size_t capacity) noexcept
		{
			if (memory && capacity)
				set_memory(memory, capacity);
		}

		~FreeListAllocator() { release(); }

		FreeListAllocator(const FreeListAllocator&) = delete;
		FreeListAllocator& operator=(const FreeListAllocator&) = delete;

		FreeListAllocator(FreeListAllocator&& other) noexcept
			: mBase(other.mBase),
			mMemory(other.mMemory),
			mCapacity(other.mCapacity),
			mUsed(other.mUsed),
			mFreeHead(other.mFreeHead),
			mOwnsMemory(other.mOwnsMemory)
		{
			other.clear_fields();
		}

		FreeListAllocator& operator=(FreeListAllocator&& other) noexcept
		{
			if (this != &other)
			{
				release();

				mBase = other.mBase;
				mMemory = other.mMemory;
				mCapacity = other.mCapacity;
				mUsed = other.mUsed;
				mFreeHead = other.mFreeHead;
				mOwnsMemory = other.mOwnsMemory;

				other.clear_fields();
			}
			return *this;
		}

		// Returns an aligned block of at least size bytes, or nullptr when no
		// free block is large enough. First-fit.
		[[nodiscard]] void* allocate(std::size_t size,
			std::size_t alignment = alignof(std::max_align_t))
		{
			if (size == 0) return nullptr;

			WCORE_ASSERT(alignment != 0 && (alignment & (alignment - 1)) == 0,
				"Free-list alignment must be a power of two");

			for (Block* block = mFreeHead; block; block = block->nextFree)
			{
				std::byte* payload{ nullptr };
				std::size_t total{ 0 };

				if (!layout(block, size, alignment, payload, total) ||
					total > block->size)
				{
					continue;
				}

				remove_free(block);

				if (block->size - total >= MinBlockSize)
				{
					auto* remainder = reinterpret_cast<Block*>(
						reinterpret_cast<std::byte*>(block) + total);

					remainder->size = block->size - total;
					remainder->prevSize = total;
					remainder->nextFree = nullptr;
					remainder->prevFree = nullptr;
					remainder->free = true;

					if (Block* after = next_block(remainder))
						after->prevSize = remainder->size;

					block->size = total;
					insert_free(remainder);
				}

				block->free = false;
				block->nextFree = nullptr;
				block->prevFree = nullptr;

				*reinterpret_cast<Block**>(
					payload - sizeof(Block*)) = block;

				mUsed += block->size;
				return payload;
			}

			return nullptr;
		}

		// Returns an allocation to the free list and coalesces neighbours.
		void deallocate(void* pointer) noexcept
		{
			if (!pointer) return;

			auto* payload = static_cast<std::byte*>(pointer);
			const std::uintptr_t address{
				reinterpret_cast<std::uintptr_t>(payload) };
			const std::uintptr_t begin{
				reinterpret_cast<std::uintptr_t>(mMemory) };
			const std::uintptr_t end{ begin + mCapacity };

			WCORE_ASSERT(mMemory != nullptr &&
				address >= begin + sizeof(Block*) && address < end,
				"Free-list pointer does not belong to this allocator");

			auto* header = *reinterpret_cast<Block**>(payload - sizeof(Block*));
			WCORE_ASSERT(header != nullptr, "Free-list block header is null");
			WCORE_ASSERT(
				reinterpret_cast<std::uintptr_t>(header) >= begin &&
				reinterpret_cast<std::uintptr_t>(header) < end,
				"Free-list block header is out of range");
			WCORE_ASSERT(!header->free, "Free-list block is already free");
			WCORE_ASSERT(header->size >= HeaderSize && header->size <= mCapacity,
				"Free-list block size is invalid");
			WCORE_ASSERT(mUsed >= header->size, "Free-list usage underflow");

			mUsed -= header->size;
			header->free = true;

			if (Block* next = next_block(header); next && next->free)
			{
				remove_free(next);
				header->size += next->size;

				if (Block* after = next_block(header))
					after->prevSize = header->size;
			}

			if (header->prevSize != 0)
			{
				auto* previous = reinterpret_cast<Block*>(
					reinterpret_cast<std::byte*>(header) - header->prevSize);

				WCORE_ASSERT(previous->free,
					"Free-list previous physical block must be free");

				remove_free(previous);
				previous->size += header->size;

				if (Block* after = next_block(previous))
					after->prevSize = previous->size;

				header = previous;
			}

			insert_free(header);
		}

		// Constructs a T in a newly allocated block. Returns nullptr when the
		// allocator is full. The allocation is released if the constructor
		// throws.
		template <typename T, typename... Args>
		[[nodiscard]] T* create(Args&&... args)
		{
			void* memory{ allocate(sizeof(T), alignof(T)) };
			if (!memory) return nullptr;

			try
			{
				return ::new (memory) T(std::forward<Args>(args)...);
			}
			catch (...)
			{
				deallocate(memory);
				throw;
			}
		}

		// Runs T's destructor and releases its block.
		template <typename T>
		void destroy(T* object)
		{
			if (!object) return;

			object->~T();
			deallocate(object);
		}

		// Rewinds the whole region into one free block. Live objects are not
		// destroyed.
		void reset() noexcept
		{
			mUsed = 0;
			mFreeHead = nullptr;

			if (!mMemory || mCapacity < MinBlockSize) return;

			mFreeHead = reinterpret_cast<Block*>(mMemory);
			mFreeHead->size = mCapacity;
			mFreeHead->prevSize = 0;
			mFreeHead->nextFree = nullptr;
			mFreeHead->prevFree = nullptr;
			mFreeHead->free = true;
		}

		// Frees the owned backing buffer, if any. External memory is left
		// untouched.
		void release() noexcept
		{
			if (mOwnsMemory && mBase)
				free_raw(mBase, HeaderAlignment);

			clear_fields();
		}

		std::size_t capacity() const noexcept { return mCapacity; }
		std::size_t used() const noexcept { return mUsed; }
		std::size_t available() const noexcept { return mCapacity - mUsed; }
		bool empty() const noexcept { return mUsed == 0; }
		bool owns_memory() const noexcept { return mOwnsMemory; }
		void* memory() const noexcept { return mMemory; }

		std::size_t free_block_count() const noexcept
		{
			std::size_t count{ 0 };
			for (const Block* block = mFreeHead; block; block = block->nextFree)
				++count;
			return count;
		}

		std::size_t largest_free_block() const noexcept
		{
			std::size_t largest{ 0 };
			for (const Block* block = mFreeHead; block; block = block->nextFree)
				largest = std::max(largest, block->size);
			return largest;
		}

	private:
		static std::size_t align_up(std::size_t value,
			std::size_t alignment) noexcept
		{
			WCORE_ASSERT(
				value <= std::numeric_limits<std::size_t>::max() - (alignment - 1),
				"Free-list size overflow");
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

		// Computes the aligned payload and the total block size required for an
		// allocation inside block.
		static bool layout(Block* block, std::size_t size,
			std::size_t alignment, std::byte*& payload,
			std::size_t& total) noexcept
		{
			const std::uintptr_t start{
				reinterpret_cast<std::uintptr_t>(block) };
			const std::uintptr_t mask{
				static_cast<std::uintptr_t>(alignment - 1) };

			if (start > std::numeric_limits<std::uintptr_t>::max() -
				HeaderSize - sizeof(Block*))
			{
				return false;
			}

			std::uintptr_t address{ start + HeaderSize + sizeof(Block*) };
			if (address > std::numeric_limits<std::uintptr_t>::max() - mask)
				return false;

			address = (address + mask) & ~mask;

			const std::size_t offset{
				static_cast<std::size_t>(address - start) };
			if (size > std::numeric_limits<std::size_t>::max() - offset)
				return false;

			total = offset + size;
			if (total > std::numeric_limits<std::size_t>::max() -
				(HeaderAlignment - 1))
			{
				return false;
			}

			total = align_up(total, HeaderAlignment);
			payload = reinterpret_cast<std::byte*>(address);
			return true;
		}

		Block* next_block(Block* block) const noexcept
		{
			auto* next = reinterpret_cast<std::byte*>(block) + block->size;
			auto* end = mMemory + mCapacity;
			return next >= end ? nullptr : reinterpret_cast<Block*>(next);
		}

		void insert_free(Block* block) noexcept
		{
			block->free = true;
			block->prevFree = nullptr;
			block->nextFree = mFreeHead;

			if (mFreeHead)
				mFreeHead->prevFree = block;

			mFreeHead = block;
		}

		void remove_free(Block* block) noexcept
		{
			WCORE_ASSERT(block->free, "Free-list block is not free");

			if (block->prevFree)
				block->prevFree->nextFree = block->nextFree;
			else
				mFreeHead = block->nextFree;

			if (block->nextFree)
				block->nextFree->prevFree = block->prevFree;

			block->nextFree = nullptr;
			block->prevFree = nullptr;
			block->free = false;
		}

		void set_memory(void* memory, std::size_t capacity) noexcept
		{
			const std::uintptr_t address{
				reinterpret_cast<std::uintptr_t>(memory) };
			const std::uintptr_t aligned{
				(address + (HeaderAlignment - 1)) & ~(HeaderAlignment - 1) };
			const std::size_t adjustment{
				static_cast<std::size_t>(aligned - address) };

			if (adjustment >= capacity)
			{
				mMemory = nullptr;
				mCapacity = 0;
			}
			else
			{
				mMemory = reinterpret_cast<std::byte*>(aligned);
				mCapacity = (capacity - adjustment) & ~(HeaderAlignment - 1);
			}

			reset();
		}

		void clear_fields() noexcept
		{
			mBase = nullptr;
			mMemory = nullptr;
			mCapacity = 0;
			mUsed = 0;
			mFreeHead = nullptr;
			mOwnsMemory = false;
		}

		void* mBase{ nullptr };
		std::byte* mMemory{ nullptr };
		std::size_t mCapacity{ 0 };
		std::size_t mUsed{ 0 };
		Block* mFreeHead{ nullptr };
		bool mOwnsMemory{ false };
	};
} // namespace wcore
