#include <wcore/test.hpp>
#include <wcore/pool.hpp>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

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

TEST(pool_allocator_starts_empty)
{
	PoolAllocator pool{ 32, 8, 4 };

	CHECK(pool.empty());
	CHECK_EQ(pool.size(), 0u);
	CHECK_EQ(pool.capacity(), 0u);
	CHECK_EQ(pool.available(), 0u);
	CHECK_EQ(pool.slab_count(), 0u);
	CHECK_EQ(pool.blocks_per_slab(), 4u);
	CHECK_EQ(pool.block_size(), 32u);
	CHECK_EQ(pool.block_alignment(), 8u);
}

TEST(pool_allocator_allocates_aligned_blocks)
{
	PoolAllocator pool{ 16, 8, 2 };
	void* block{ pool.allocate() };

	REQUIRE(block != nullptr);
	CHECK_EQ(reinterpret_cast<std::uintptr_t>(block) % 8, 0u);
	CHECK_EQ(pool.size(), 1u);
	CHECK_EQ(pool.capacity(), 2u);
	CHECK_EQ(pool.available(), 1u);
	CHECK_EQ(pool.slab_count(), 1u);
	CHECK(!pool.empty());
}

TEST(pool_allocator_returns_distinct_blocks)
{
	PoolAllocator pool{ 16, 8, 2 };

	void* first{ pool.allocate() };
	void* second{ pool.allocate() };

	REQUIRE(first != nullptr);
	REQUIRE(second != nullptr);
	CHECK(first != second);
	CHECK_EQ(pool.size(), 2u);
	CHECK(pool.available() == 0u);

	pool.deallocate(first);
	pool.deallocate(second);

	CHECK(pool.empty());
	CHECK_EQ(pool.size(), 0u);
	CHECK_EQ(pool.available(), pool.capacity());
}

TEST(pool_allocator_reuses_freed_blocks)
{
	PoolAllocator pool{ 16, 8, 2 };

	void* first{ pool.allocate() };
	void* second{ pool.allocate() };
	pool.deallocate(first);

	void* reused{ pool.allocate() };

	CHECK_EQ(reinterpret_cast<std::uintptr_t>(reused),
		reinterpret_cast<std::uintptr_t>(first));
	CHECK(reused != second);
	CHECK_EQ(pool.size(), 2u);
}

TEST(pool_allocator_grows_in_slabs)
{
	PoolAllocator pool{ 16, 8, 2 };

	REQUIRE(pool.allocate() != nullptr);
	REQUIRE(pool.allocate() != nullptr);
	CHECK_EQ(pool.slab_count(), 1u);

	REQUIRE(pool.allocate() != nullptr);
	CHECK_EQ(pool.size(), 3u);
	CHECK_EQ(pool.slab_count(), 2u);
	CHECK_EQ(pool.capacity(), 4u);
	CHECK_EQ(pool.available(), 1u);
}

TEST(pool_allocator_slab_blocks_are_all_usable)
{
	PoolAllocator pool{ 8, 8, 4 };

	for (int i{ 0 }; i < 4; ++i)
		REQUIRE(pool.allocate() != nullptr);

	CHECK_EQ(pool.size(), 4u);
	CHECK_EQ(pool.slab_count(), 1u);

	REQUIRE(pool.allocate() != nullptr);
	CHECK_EQ(pool.slab_count(), 2u);
}

TEST(pool_allocator_reset_returns_all_blocks)
{
	PoolAllocator pool{ 16, 8, 4 };
	REQUIRE(pool.allocate() != nullptr);
	REQUIRE(pool.allocate() != nullptr);

	const std::size_t capacity{ pool.capacity() };
	const std::size_t slabs{ pool.slab_count() };

	pool.reset();

	CHECK(pool.empty());
	CHECK_EQ(pool.size(), 0u);
	CHECK_EQ(pool.available(), capacity);
	CHECK_EQ(pool.capacity(), capacity);
	CHECK_EQ(pool.slab_count(), slabs);

	for (std::size_t i{ 0 }; i < capacity; ++i)
		REQUIRE(pool.allocate() != nullptr);

	CHECK_EQ(pool.size(), capacity);
}

TEST(pool_allocator_release_frees_slabs)
{
	PoolAllocator pool{ 16, 8, 2 };
	REQUIRE(pool.allocate() != nullptr);

	pool.release();

	CHECK(pool.empty());
	CHECK_EQ(pool.size(), 0u);
	CHECK_EQ(pool.capacity(), 0u);
	CHECK_EQ(pool.available(), 0u);
	CHECK_EQ(pool.slab_count(), 0u);

	REQUIRE(pool.allocate() != nullptr);
	CHECK_EQ(pool.capacity(), 2u);
}

TEST(pool_allocator_reserve_preallocates)
{
	PoolAllocator pool{ 16, 8, 4 };

	pool.reserve(10);

	CHECK(pool.empty());
	CHECK_EQ(pool.size(), 0u);
	CHECK_EQ(pool.capacity(), 12u);
	CHECK_EQ(pool.available(), pool.capacity());
	CHECK_EQ(pool.slab_count(), 3u);

	pool.reserve(8);
	CHECK_EQ(pool.capacity(), 12u);
}

TEST(pool_allocator_honors_alignment)
{
	for (const std::size_t alignment : { 8u, 16u, 32u, 64u, 128u })
	{
		PoolAllocator pool{ 24, alignment, 2 };

		for (int i{ 0 }; i < 4; ++i)
		{
			void* block{ pool.allocate() };
			REQUIRE(block != nullptr);
			CHECK_EQ(reinterpret_cast<std::uintptr_t>(block) % alignment, 0u);
		}

		CHECK_EQ(pool.block_alignment(),
			std::max(alignment, alignof(void*)));
		CHECK_EQ(pool.block_size() % pool.block_alignment(), 0u);
	}
}

TEST(pool_allocator_rounds_small_blocks_to_pointer_size)
{
	PoolAllocator pool{ 1, 8, 2 };

	CHECK(pool.block_size() >= sizeof(void*));
	CHECK_EQ(pool.block_size() % pool.block_alignment(), 0u);
}

TEST(pool_allocator_blocks_are_writable)
{
	struct Value
	{
		int first;
		int second;
	};

	PoolAllocator pool{ sizeof(Value), alignof(Value), 2 };
	auto* first = static_cast<Value*>(pool.allocate());
	auto* second = static_cast<Value*>(pool.allocate());

	REQUIRE(first != nullptr);
	REQUIRE(second != nullptr);

	first->first = 1;
	first->second = 2;
	second->first = 3;
	second->second = 4;

	CHECK_EQ(first->first, 1);
	CHECK_EQ(first->second, 2);
	CHECK_EQ(second->first, 3);
	CHECK_EQ(second->second, 4);
}

TEST(pool_allocator_move_preserves_blocks)
{
	PoolAllocator pool{ 16, 8, 2 };
	void* block{ pool.allocate() };

	PoolAllocator moved{ std::move(pool) };

	CHECK_EQ(moved.size(), 1u);
	CHECK_EQ(moved.capacity(), 2u);
	CHECK_EQ(moved.block_size(), 16u);

	CHECK(pool.empty());
	CHECK_EQ(pool.capacity(), 0u);
	CHECK_EQ(pool.slab_count(), 0u);

	moved.deallocate(block);
	CHECK(moved.empty());

	REQUIRE(pool.allocate() != nullptr);
	CHECK_EQ(pool.capacity(), 2u);
}

TEST(pool_allocator_move_assignment_replaces_state)
{
	PoolAllocator source{ 16, 8, 2 };
	void* block{ source.allocate() };

	PoolAllocator target{ 32, 8, 4 };
	REQUIRE(target.allocate() != nullptr);

	target = std::move(source);

	CHECK_EQ(target.size(), 1u);
	CHECK_EQ(target.capacity(), 2u);
	CHECK_EQ(target.block_size(), 16u);

	target.deallocate(block);
	CHECK(target.empty());
}

TEST(pool_create_and_destroy)
{
	Pool<int> pool{ 2 };
	int* value = pool.create(42);

	REQUIRE(value != nullptr);
	CHECK_EQ(*value, 42);
	CHECK_EQ(pool.size(), 1u);
	CHECK(!pool.empty());

	pool.destroy(value);

	CHECK_EQ(pool.size(), 0u);
	CHECK(pool.empty());
}

TEST(pool_constructs_and_destroys_objects)
{
	int alive{ 0 };
	Pool<Tracked> pool{ 2 };

	Tracked* first = pool.create(&alive);
	Tracked* second = pool.create(&alive);

	REQUIRE(first != nullptr);
	REQUIRE(second != nullptr);
	CHECK_EQ(alive, 2);
	CHECK_EQ(pool.size(), 2u);

	pool.destroy(first);
	CHECK_EQ(alive, 1);

	pool.destroy(second);
	CHECK_EQ(alive, 0);
	CHECK(pool.empty());
}

TEST(pool_reuses_destroyed_slots)
{
	Pool<int> pool{ 2 };
	int* first = pool.create(1);
	REQUIRE(first != nullptr);
	pool.destroy(first);

	int* reused = pool.create(2);

	REQUIRE(reused != nullptr);
	CHECK_EQ(reinterpret_cast<std::uintptr_t>(reused),
		reinterpret_cast<std::uintptr_t>(first));
	CHECK_EQ(*reused, 2);
}

TEST(pool_throwing_constructor_returns_block)
{
	Pool<Thrower> pool{ 2 };

	bool threw{ false };
	try
	{
		(void)pool.create(true);
	}
	catch (const std::runtime_error&)
	{
		threw = true;
	}

	CHECK(threw);
	CHECK_EQ(pool.size(), 0u);

	Thrower* object = pool.create(false);
	REQUIRE(object != nullptr);
	CHECK_EQ(pool.size(), 1u);
}

TEST(pool_supports_move_only_types)
{
	struct MoveOnly
	{
		int value;

		explicit MoveOnly(int v) : value(v) {}
		MoveOnly(const MoveOnly&) = delete;
		MoveOnly& operator=(const MoveOnly&) = delete;
		MoveOnly(MoveOnly&&) noexcept = default;
		MoveOnly& operator=(MoveOnly&&) noexcept = default;
	};

	Pool<MoveOnly> pool;

	MoveOnly* object = pool.create(5);
	REQUIRE(object != nullptr);
	CHECK_EQ(object->value, 5);

	object->value = 6;
	CHECK_EQ(object->value, 6);

	pool.destroy(object);
	CHECK(pool.empty());
}

TEST(pool_supports_over_aligned_types)
{
	Pool<AlignedValue> pool{ 2 };
	AlignedValue* value = pool.create(42);

	REQUIRE(value != nullptr);
	CHECK_EQ(reinterpret_cast<std::uintptr_t>(value) % alignof(AlignedValue), 0u);
	CHECK_EQ(value->value, 42);
	CHECK(pool.block_size() % alignof(AlignedValue) == 0u);
}

TEST(pool_raw_allocate_and_deallocate)
{
	Pool<int> pool{ 2 };
	int* value = pool.allocate();

	REQUIRE(value != nullptr);
	*value = 9;
	CHECK_EQ(*value, 9);
	CHECK_EQ(pool.size(), 1u);

	pool.deallocate(value);
	CHECK_EQ(pool.size(), 0u);
}

TEST(pool_reset_and_release)
{
	Pool<int> pool{ 4 };
	REQUIRE(pool.create(1) != nullptr);

	const std::size_t capacity{ pool.capacity() };

	pool.reset();
	CHECK(pool.empty());
	CHECK_EQ(pool.size(), 0u);
	CHECK_EQ(pool.capacity(), capacity);

	pool.release();
	CHECK_EQ(pool.capacity(), 0u);
	CHECK_EQ(pool.slab_count(), 0u);
}

TEST(pool_move_preserves_blocks)
{
	Pool<int> pool{ 2 };
	int* value = pool.create(7);
	REQUIRE(value != nullptr);

	Pool<int> moved{ std::move(pool) };

	CHECK_EQ(*value, 7);
	CHECK_EQ(moved.size(), 1u);
	CHECK(pool.empty());
	CHECK_EQ(pool.capacity(), 0u);

	int* other = pool.create(8);
	REQUIRE(other != nullptr);
	CHECK_EQ(*other, 8);
}
