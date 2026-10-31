#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <utility>
#include <vector>

#include <wcore/assert.hpp>

namespace wcore
{
	// A bump/linear allocator.
	class Arena
	{
	public:
		static constexpr std::size_t DefaultBlockSize{ 64 * 1024 };

		// A saved position in the arena. Restoring it releases everything
		// allocated after the marker was taken.
		class Marker
		{
		public:
			Marker() = default;

		private:
			friend class Arena;

			Marker(std::size_t block, std::size_t used)
				: block(block), used(used) {}

			std::size_t block{ 0 };
			std::size_t used{ 0 };
		};

		// Restores its marker on destruction. Scopes can be nested.
		class Scope
		{
		public:
			explicit Scope(Arena& arena)
				: mArena(&arena), mMarker(arena.mark()) {}

			~Scope() { end(); }

			Scope(const Scope&) = delete;
			Scope& operator=(const Scope&) = delete;

			Scope(Scope&& other) noexcept
				: mArena(other.mArena), mMarker(other.mMarker)
			{
				other.mArena = nullptr;
			}

			Scope& operator=(Scope&& other) noexcept
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

			// Restores immediately. Called by the destructor as well.
			void end()
			{
				if (mArena)
				{
					mArena->restore(mMarker);
					mArena = nullptr;
				}
			}

		private:
			Arena* mArena{ nullptr };
			Marker mMarker;
		};

		explicit Arena(std::size_t blockSize = DefaultBlockSize)
			: mBlockSize(blockSize == 0 ? DefaultBlockSize : blockSize) {}

		~Arena() = default;

		Arena(const Arena&) = delete;
		Arena& operator=(const Arena&) = delete;

		Arena(Arena&& other) noexcept
			: mBlocks(std::move(other.mBlocks)),
			mBlockSize(other.mBlockSize),
			mCurrent(other.mCurrent)
		{
			other.mBlocks.clear();
			other.mCurrent = 0;
		}

		Arena& operator=(Arena&& other) noexcept
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

		// Returns aligned storage for size bytes, or nullptr for size == 0.
		[[nodiscard]] void* allocate(std::size_t size,
			std::size_t alignment = alignof(std::max_align_t))
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

			if (void* memory = try_allocate(*mBlocks[mCurrent], size, alignment))
				return memory;

			void* memory = try_allocate(*acquire_block(required), size, alignment);
			WCORE_ASSERT(memory != nullptr, "Arena block is too small for allocation");
			return memory;
		}

		// Constructs a T inside the arena. Rolls the arena back if the
		// constructor throws.
		template <typename T, typename... Args>
		[[nodiscard]] T* create(Args&&... args)
		{
			const Marker checkpoint{ mark() };
			void* memory{ allocate(sizeof(T), alignof(T)) };

			try
			{
				return ::new (memory) T(std::forward<Args>(args)...);
			}
			catch (...)
			{
				restore(checkpoint);
				throw;
			}
		}

		// Runs a destructor without reclaiming the storage.
		template <typename T>
		void destroy(T* object)
		{
			if (object) object->~T();
		}

		// Rewinds to the current position, keeping all blocks for reuse.
		void reset()
		{
			for (auto& block : mBlocks)
				block->used = 0;
			mCurrent = 0;
		}

		// Frees every block.
		void release()
		{
			mBlocks.clear();
			mCurrent = 0;
		}

		[[nodiscard]] Marker mark() const
		{
			if (mBlocks.empty()) return Marker{};
			return Marker{ mCurrent, mBlocks[mCurrent]->used };
		}

		// Rewinds to marker. Blocks allocated after it are kept for reuse.
		void restore(Marker marker)
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

		// Total storage reserved from the heap.
		std::size_t capacity() const
		{
			std::size_t total{ 0 };
			for (const auto& block : mBlocks)
				total += block->capacity;
			return total;
		}

		// Bytes currently handed out, including alignment padding.
		std::size_t used() const
		{
			std::size_t total{ 0 };
			for (const auto& block : mBlocks)
				total += block->used;
			return total;
		}

		std::size_t available() const { return capacity() - used(); }
		std::size_t block_count() const { return mBlocks.size(); }
		std::size_t block_size() const { return mBlockSize; }
		bool empty() const { return used() == 0; }

	private:
		struct Block
		{
			explicit Block(std::size_t bytes)
				: data(static_cast<std::byte*>(::operator new(bytes))),
				capacity(bytes) {}

			~Block() { ::operator delete(data); }

			Block(const Block&) = delete;
			Block& operator=(const Block&) = delete;

			std::byte* data;
			std::size_t capacity;
			std::size_t used{ 0 };
		};

		static void* try_allocate(Block& block, std::size_t size,
			std::size_t alignment)
		{
			void* pointer{ block.data + block.used };
			std::size_t space{ block.capacity - block.used };

			void* aligned{ std::align(alignment, size, pointer, space) };
			if (!aligned) return nullptr;

			const auto offset{ static_cast<std::size_t>(
				static_cast<std::byte*>(aligned) - block.data) };
			block.used = offset + size;
			return aligned;
		}

		void append_block(std::size_t capacity)
		{
			mBlocks.push_back(std::make_unique<Block>(capacity));
			mCurrent = mBlocks.size() - 1;
		}

		Block* acquire_block(std::size_t required)
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

		std::vector<std::unique_ptr<Block>> mBlocks;
		std::size_t mBlockSize;
		std::size_t mCurrent{ 0 };
	};
} // namespace wcore
