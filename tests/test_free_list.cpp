#include <wcore/test.hpp>
#include <wcore/free_list.hpp>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace wcore;
using namespace test;

namespace
{
	struct Tracked
	{
		int* count;

		explicit Tracked(int* c) : count(c) { ++*count; }
		~Tracked() { --*count; }

		Tracked(const Tracked&) = delete;
		Tracked& operator=(const Tracked&) = delete;
	};

	struct Thrower
	{
		explicit Thrower(bool shouldThrow)
		{
			if (shouldThrow) throw std::runtime_error("constructor failed");
		}
	};

	struct alignas(64) AlignedValue
	{
		explicit AlignedValue(int v) : value(v) {}
		int value{ 0 };
	};
}

TEST(free_list_allocator_starts_empty)
{
	FreeListAllocator allocator{ 1024 };

	CHECK(allocator.empty());
	CHECK(allocator.owns_memory());
	CHECK(allocator.memory() != nullptr);
	CHECK_EQ(allocator.capacity(), 1024u);
	CHECK_EQ(allocator.used(), 0u);
	CHECK_EQ(allocator.available(), 1024u);
	CHECK_EQ(allocator.free_block_count(), 1u);
	CHECK_EQ(allocator.largest_free_block(), 1024u);
}

TEST(free_list_allocator_allocates_aligned_memory)
{
	FreeListAllocator allocator{ 1024 };
	void* memory{ allocator.allocate(32) };

	REQUIRE(memory != nullptr);
	CHECK_EQ(reinterpret_cast<std::uintptr_t>(memory) %
		alignof(std::max_align_t), 0u);
	CHECK(allocator.used() > 0u);
	CHECK(allocator.used() <= allocator.capacity());
	CHECK(!allocator.empty());
}

TEST(free_list_allocator_honors_alignment)
{
	for (const std::size_t alignment : { 1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u })
	{
		FreeListAllocator allocator{ 4096 };

		for (int i{ 0 }; i < 4; ++i)
		{
			void* memory{ allocator.allocate(24, alignment) };
			REQUIRE(memory != nullptr);
			CHECK_EQ(reinterpret_cast<std::uintptr_t>(memory) % alignment, 0u);
		}
	}
}

TEST(free_list_allocator_returns_distinct_writable_blocks)
{
	FreeListAllocator allocator{ 1024 };

	auto* first = static_cast<int*>(allocator.allocate(sizeof(int), alignof(int)));
	auto* second = static_cast<int*>(allocator.allocate(sizeof(int), alignof(int)));

	REQUIRE(first != nullptr);
	REQUIRE(second != nullptr);
	CHECK(first != second);

	*first = 11;
	*second = 22;
	CHECK_EQ(*first, 11);
	CHECK_EQ(*second, 22);
}

TEST(free_list_allocator_zero_size_returns_null)
{
	FreeListAllocator allocator{ 1024 };

	CHECK(allocator.allocate(0) == nullptr);
	CHECK(allocator.empty());
	CHECK_EQ(allocator.used(), 0u);
}

TEST(free_list_allocator_returns_null_when_full)
{
	FreeListAllocator allocator{ 64 };

	CHECK(allocator.allocate(1024) == nullptr);
	CHECK(allocator.empty());
	CHECK_EQ(allocator.used(), 0u);
}

TEST(free_list_allocator_reuses_freed_block)
{
	FreeListAllocator allocator{ 4096 };
	void* first{ allocator.allocate(64) };
	void* second{ allocator.allocate(64) };

	REQUIRE(first != nullptr);
	REQUIRE(second != nullptr);
	CHECK(first != second);

	allocator.deallocate(first);
	void* reused{ allocator.allocate(64) };

	CHECK_EQ(reinterpret_cast<std::uintptr_t>(reused),
		reinterpret_cast<std::uintptr_t>(first));
	CHECK(reused != second);
}

TEST(free_list_allocator_coalesces_with_following_block)
{
	FreeListAllocator allocator{ 1024 };
	const std::size_t usedBefore{ allocator.used() };
	void* memory{ allocator.allocate(128) };

	REQUIRE(memory != nullptr);
	CHECK(allocator.used() > usedBefore);
	CHECK_EQ(allocator.free_block_count(), 1u);

	allocator.deallocate(memory);

	CHECK_EQ(allocator.used(), usedBefore);
	CHECK_EQ(allocator.free_block_count(), 1u);
	CHECK_EQ(allocator.largest_free_block(), allocator.capacity());
}

TEST(free_list_allocator_coalesces_adjacent_blocks)
{
	FreeListAllocator allocator{ 512 };

	std::vector<void*> blocks;
	while (void* block = allocator.allocate(64))
		blocks.push_back(block);

	REQUIRE(blocks.size() >= 2u);

	void* first{ blocks[0] };
	void* second{ blocks[1] };
	allocator.deallocate(first);
	allocator.deallocate(second);

	// Neither original block is large enough for 128 bytes on its own.
	void* merged{ allocator.allocate(128) };
	REQUIRE(merged != nullptr);
	CHECK_EQ(reinterpret_cast<std::uintptr_t>(merged),
		reinterpret_cast<std::uintptr_t>(first));
}

TEST(free_list_allocator_reset_restores_single_block)
{
	FreeListAllocator allocator{ 1024 };
	REQUIRE(allocator.allocate(128) != nullptr);
	REQUIRE(allocator.allocate(64) != nullptr);

	const std::size_t capacity{ allocator.capacity() };

	allocator.reset();

	CHECK(allocator.empty());
	CHECK_EQ(allocator.used(), 0u);
	CHECK_EQ(allocator.capacity(), capacity);
	CHECK_EQ(allocator.free_block_count(), 1u);
	CHECK_EQ(allocator.largest_free_block(), capacity);

	REQUIRE(allocator.allocate(128) != nullptr);
}

TEST(free_list_allocator_release_frees_owned_memory)
{
	FreeListAllocator allocator{ 1024 };
	REQUIRE(allocator.allocate(64) != nullptr);

	allocator.release();

	CHECK(!allocator.owns_memory());
	CHECK(allocator.memory() == nullptr);
	CHECK_EQ(allocator.capacity(), 0u);
	CHECK_EQ(allocator.used(), 0u);
	CHECK_EQ(allocator.free_block_count(), 0u);
	CHECK(allocator.allocate(16) == nullptr);
}

TEST(free_list_allocator_uses_external_memory)
{
	alignas(std::max_align_t) std::byte buffer[1024]{};

	FreeListAllocator allocator{ buffer, sizeof(buffer) };

	CHECK(!allocator.owns_memory());
	CHECK_EQ(allocator.memory(), static_cast<void*>(buffer));
	CHECK_EQ(allocator.capacity(), 1024u);

	void* memory{ allocator.allocate(64) };
	REQUIRE(memory != nullptr);
	allocator.deallocate(memory);
	CHECK(allocator.empty());

	allocator.release();
	CHECK_EQ(allocator.capacity(), 0u);
}

TEST(free_list_allocator_creates_and_destroys_objects)
{
	int alive{ 0 };
	FreeListAllocator allocator{ 1024 };

	Tracked* first = allocator.create<Tracked>(&alive);
	Tracked* second = allocator.create<Tracked>(&alive);

	REQUIRE(first != nullptr);
	REQUIRE(second != nullptr);
	CHECK_EQ(alive, 2);
	CHECK(!allocator.empty());

	allocator.destroy(first);
	CHECK_EQ(alive, 1);

	allocator.destroy(second);
	CHECK_EQ(alive, 0);
	CHECK(allocator.empty());
}

TEST(free_list_allocator_throwing_constructor_reclaims)
{
	FreeListAllocator allocator{ 1024 };
	const std::size_t usedBefore{ allocator.used() };

	bool threw{ false };
	try
	{
		(void)allocator.create<Thrower>(true);
	}
	catch (const std::runtime_error&)
	{
		threw = true;
	}

	CHECK(threw);
	CHECK_EQ(allocator.used(), usedBefore);
	CHECK_EQ(allocator.free_block_count(), 1u);
	CHECK_EQ(allocator.largest_free_block(), allocator.capacity());
}

TEST(free_list_allocator_supports_over_aligned_types)
{
	FreeListAllocator allocator{ 2048 };
	AlignedValue* value = allocator.create<AlignedValue>(42);

	REQUIRE(value != nullptr);
	CHECK_EQ(reinterpret_cast<std::uintptr_t>(value) % alignof(AlignedValue), 0u);
	CHECK_EQ(value->value, 42);
}

TEST(free_list_allocator_create_returns_null_when_full)
{
	FreeListAllocator allocator{ 8 };

	CHECK(allocator.create<int>(1) == nullptr);
	CHECK(allocator.empty());
}

TEST(free_list_allocator_move_preserves_allocations)
{
	FreeListAllocator allocator{ 1024 };
	int* value = allocator.create<int>(7);
	REQUIRE(value != nullptr);

	const std::size_t used{ allocator.used() };

	FreeListAllocator moved{ std::move(allocator) };

	CHECK_EQ(*value, 7);
	CHECK_EQ(moved.used(), used);
	CHECK(!moved.empty());
	CHECK(moved.owns_memory());

	CHECK(allocator.empty());
	CHECK_EQ(allocator.capacity(), 0u);
	CHECK(allocator.allocate(16) == nullptr);

	moved.destroy(value);
	CHECK(moved.empty());
}

TEST(free_list_allocator_move_assignment_preserves_allocations)
{
	FreeListAllocator source{ 1024 };
	int* value = source.create<int>(7);
	REQUIRE(value != nullptr);
	const std::size_t used{ source.used() };

	FreeListAllocator target{ 256 };
	REQUIRE(target.allocate(16) != nullptr);

	target = std::move(source);

	CHECK_EQ(*value, 7);
	CHECK(target.capacity() >= 1024u);
	CHECK_EQ(target.used(), used);
	CHECK(source.empty());
	CHECK_EQ(source.capacity(), 0u);
}
