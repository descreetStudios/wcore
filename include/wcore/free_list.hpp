#pragma once

#include <cstddef>
#include <new>
#include <utility>

namespace wcore
{
	class FreeListAllocator
	{
	private:
		struct alignas(std::max_align_t) Block
		{
			std::size_t size{ 0 };
			std::size_t prevSize{ 0 };
			Block* nextFree{ nullptr };
			Block* prevFree{ nullptr };
			bool free{ false };
		};

		static constexpr std::size_t HeaderSize{ sizeof(Block) };
		static constexpr std::size_t HeaderAlignment{ alignof(Block) };
		static constexpr std::size_t MinBlockSize{ sizeof(Block) };

	public:
		explicit FreeListAllocator(std::size_t capacity);

		FreeListAllocator(void* memory, std::size_t capacity) noexcept;

		~FreeListAllocator();

		FreeListAllocator(const FreeListAllocator&) = delete;
		FreeListAllocator& operator=(const FreeListAllocator&) = delete;

		FreeListAllocator(FreeListAllocator&& other) noexcept;
		FreeListAllocator& operator=(FreeListAllocator&& other) noexcept;

		[[nodiscard]] void* allocate(std::size_t size,
			std::size_t alignment = alignof(std::max_align_t));

		void deallocate(void* pointer) noexcept;

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

		template <typename T>
		void destroy(T* object)
		{
			if (!object) return;

			object->~T();
			deallocate(object);
		}

		void reset() noexcept;
		void release() noexcept;

		std::size_t capacity() const noexcept { return mCapacity; }
		std::size_t used() const noexcept { return mUsed; }
		std::size_t available() const noexcept { return mCapacity - mUsed; }
		bool empty() const noexcept { return mUsed == 0; }
		bool owns_memory() const noexcept { return mOwnsMemory; }
		void* memory() const noexcept { return mMemory; }

		std::size_t free_block_count() const noexcept;

		std::size_t largest_free_block() const noexcept;

	private:
		static std::size_t align_up(std::size_t value,
			std::size_t alignment) noexcept;
		static void* allocate_raw(std::size_t bytes, std::size_t alignment);
		static void free_raw(void* data, std::size_t alignment) noexcept;
		static bool layout(Block* block, std::size_t size,
			std::size_t alignment, std::byte*& payload,
			std::size_t& total) noexcept;
		Block* next_block(Block* block) const noexcept;
		void insert_free(Block* block) noexcept;
		void remove_free(Block* block) noexcept;
		void set_memory(void* memory, std::size_t capacity) noexcept;
		void clear_fields() noexcept;

		void* mBase{ nullptr };
		std::byte* mMemory{ nullptr };
		std::size_t mCapacity{ 0 };
		std::size_t mUsed{ 0 };
		Block* mFreeHead{ nullptr };
		bool mOwnsMemory{ false };
	};
}
