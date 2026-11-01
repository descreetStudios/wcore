#pragma once

#include <cstddef>
#include <memory>
#include <new>
#include <utility>
#include <vector>

namespace wcore
{
	class Arena
	{
	public:
		static constexpr std::size_t DefaultBlockSize{ 64 * 1024 };

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

		class Scope
		{
		public:
			explicit Scope(Arena& arena)
				: mArena(&arena), mMarker(arena.mark()) {}

			~Scope() { end(); }

			Scope(const Scope&) = delete;
			Scope& operator=(const Scope&) = delete;

			Scope(Scope&& other) noexcept;

			Scope& operator=(Scope&& other) noexcept;

			// Restores immediately. Called by the destructor as well.
			void end();

		private:
			Arena* mArena{ nullptr };
			Marker mMarker;
		};

		explicit Arena(std::size_t blockSize = DefaultBlockSize)
			: mBlockSize(blockSize == 0 ? DefaultBlockSize : blockSize) {}

		~Arena() = default;

		Arena(const Arena&) = delete;
		Arena& operator=(const Arena&) = delete;

		Arena(Arena&& other) noexcept;
		Arena& operator=(Arena&& other) noexcept;

		[[nodiscard]] void* allocate(std::size_t size,
			std::size_t alignment = alignof(std::max_align_t));

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

		template <typename T>
		void destroy(T* object)
		{
			if (object) object->~T();
		}

		void reset();
		void release();

		[[nodiscard]] Marker mark() const;

		// Rewinds to marker. Blocks allocated after it are kept for reuse.
		void restore(Marker marker);

		std::size_t capacity() const;
		std::size_t used() const;

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
			std::size_t alignment);
		void append_block(std::size_t capacity);
		Block* acquire_block(std::size_t required);

		std::vector<std::unique_ptr<Block>> mBlocks;
		std::size_t mBlockSize;
		std::size_t mCurrent{ 0 };
	};
}
