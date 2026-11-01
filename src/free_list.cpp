#include <wcore/free_list.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>

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

	FreeListAllocator::FreeListAllocator(std::size_t capacity)
	{
		if (capacity == 0) return;

		mOwnsMemory = true;
		mBase = allocate_raw(capacity, HeaderAlignment);
		set_memory(mBase, capacity);
	}

	FreeListAllocator::FreeListAllocator(void* memory,
		std::size_t capacity) noexcept
	{
		if (memory && capacity)
			set_memory(memory, capacity);
	}

	FreeListAllocator::~FreeListAllocator()
	{
		release();
	}

	FreeListAllocator::FreeListAllocator(FreeListAllocator&& other) noexcept
		: mBase(other.mBase),
		mMemory(other.mMemory),
		mCapacity(other.mCapacity),
		mUsed(other.mUsed),
		mFreeHead(other.mFreeHead),
		mOwnsMemory(other.mOwnsMemory)
	{
		other.clear_fields();
	}

	FreeListAllocator& FreeListAllocator::operator=(
		FreeListAllocator&& other) noexcept
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

	void* FreeListAllocator::allocate(std::size_t size, std::size_t alignment)
	{
		if (size == 0) return nullptr;

		WCORE_ASSERT(alignment != 0 && (alignment & (alignment - 1)) == 0,
			"Free-list alignment must be a power of two");

		for (Block* block{ mFreeHead }; block; block = block->nextFree)
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

				if (Block* after{ next_block(remainder) })
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

	void FreeListAllocator::deallocate(void* pointer) noexcept
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

		if (Block* next{ next_block(header) }; next && next->free)
		{
			remove_free(next);
			header->size += next->size;

			if (Block* after{ next_block(header) })
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

			if (Block* after{ next_block(previous) })
				after->prevSize = previous->size;

			header = previous;
		}

		insert_free(header);
	}

	void FreeListAllocator::reset() noexcept
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

	void FreeListAllocator::release() noexcept
	{
		if (mOwnsMemory && mBase)
			free_raw(mBase, HeaderAlignment);

		clear_fields();
	}

	std::size_t FreeListAllocator::free_block_count() const noexcept
	{
		std::size_t count{ 0 };
		for (const Block* block = mFreeHead; block; block = block->nextFree)
			++count;
		return count;
	}

	std::size_t FreeListAllocator::largest_free_block() const noexcept
	{
		std::size_t largest{ 0 };
		for (const Block* block = mFreeHead; block; block = block->nextFree)
			largest = std::max(largest, block->size);
		return largest;
	}

	std::size_t FreeListAllocator::align_up(std::size_t value,
		std::size_t alignment) noexcept
	{
		WCORE_ASSERT(
			value <= std::numeric_limits<std::size_t>::max() - (alignment - 1),
			"Free-list size overflow");
		return (value + alignment - 1) & ~(alignment - 1);
	}

	void* FreeListAllocator::allocate_raw(std::size_t bytes,
		std::size_t alignment)
	{
		if (alignment > DefaultNewAlignment)
			return ::operator new(bytes, std::align_val_t(alignment));

		return ::operator new(bytes);
	}

	void FreeListAllocator::free_raw(void* data,
		std::size_t alignment) noexcept
	{
		if (alignment > DefaultNewAlignment)
			::operator delete(data, std::align_val_t(alignment));
		else
			::operator delete(data);
	}

	bool FreeListAllocator::layout(Block* block, std::size_t size,
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

	FreeListAllocator::Block* FreeListAllocator::next_block(
		Block* block) const noexcept
	{
		auto* next = reinterpret_cast<std::byte*>(block) + block->size;
		auto* end = mMemory + mCapacity;
		return next >= end ? nullptr : reinterpret_cast<Block*>(next);
	}

	void FreeListAllocator::insert_free(Block* block) noexcept
	{
		block->free = true;
		block->prevFree = nullptr;
		block->nextFree = mFreeHead;

		if (mFreeHead)
			mFreeHead->prevFree = block;

		mFreeHead = block;
	}

	void FreeListAllocator::remove_free(Block* block) noexcept
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

	void FreeListAllocator::set_memory(void* memory,
		std::size_t capacity) noexcept
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

	void FreeListAllocator::clear_fields() noexcept
	{
		mBase = nullptr;
		mMemory = nullptr;
		mCapacity = 0;
		mUsed = 0;
		mFreeHead = nullptr;
		mOwnsMemory = false;
	}
}