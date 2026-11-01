#include <wcore/test.hpp>
#include <wcore/arena.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
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

TEST(arena_starts_empty)
{
	Arena arena;

	CHECK(arena.empty());
	CHECK_EQ(arena.used(), 0u);
	CHECK_EQ(arena.capacity(), 0u);
	CHECK_EQ(arena.available(), 0u);
	CHECK_EQ(arena.block_count(), 0u);
	CHECK_EQ(arena.block_size(), Arena::DefaultBlockSize);
}

TEST(arena_allocate_returns_aligned_storage)
{
	Arena arena{ 256 };
	void* memory{ arena.allocate(16) };

	REQUIRE(memory != nullptr);
	CHECK_EQ(arena.block_count(), 1u);
	CHECK(arena.capacity() >= 256u);
	CHECK(arena.used() >= 16u);
	CHECK(!arena.empty());
	CHECK_EQ(arena.available(), arena.capacity() - arena.used());
	CHECK_EQ(reinterpret_cast<std::uintptr_t>(memory) % alignof(std::max_align_t), 0u);
}

TEST(arena_allocate_honors_alignment)
{
	Arena arena{ 1024 };

	for (const std::size_t alignment : { 1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u })
	{
		void* memory{ arena.allocate(3, alignment) };
		REQUIRE(memory != nullptr);
		CHECK_EQ(reinterpret_cast<std::uintptr_t>(memory) % alignment, 0u);
	}
}

TEST(arena_allocations_are_distinct_and_writable)
{
	Arena arena{ 256 };

	auto* first = static_cast<int*>(arena.allocate(sizeof(int), alignof(int)));
	auto* second = static_cast<int*>(arena.allocate(sizeof(int), alignof(int)));

	REQUIRE(first != nullptr);
	REQUIRE(second != nullptr);
	CHECK(first != second);

	*first = 11;
	*second = 22;
	CHECK_EQ(*first, 11);
	CHECK_EQ(*second, 22);
}

TEST(arena_zero_size_allocation_returns_null)
{
	Arena arena;

	CHECK(arena.allocate(0) == nullptr);
	CHECK(arena.empty());
	CHECK_EQ(arena.block_count(), 0u);
}

TEST(arena_reset_reuses_storage)
{
	Arena arena{ 128 };
	void* first{ arena.allocate(32, 1) };
	const std::size_t capacity{ arena.capacity() };
	const std::size_t blocks{ arena.block_count() };

	arena.reset();

	CHECK(arena.empty());
	CHECK_EQ(arena.used(), 0u);
	CHECK_EQ(arena.capacity(), capacity);
	CHECK_EQ(arena.block_count(), blocks);

	void* second{ arena.allocate(32, 1) };
	CHECK_EQ(reinterpret_cast<std::uintptr_t>(first),
		reinterpret_cast<std::uintptr_t>(second));
}

TEST(arena_release_frees_all_storage)
{
	Arena arena{ 128 };
	REQUIRE(arena.allocate(64, 1) != nullptr);

	arena.release();

	CHECK(arena.empty());
	CHECK_EQ(arena.used(), 0u);
	CHECK_EQ(arena.capacity(), 0u);
	CHECK_EQ(arena.block_count(), 0u);
	REQUIRE(arena.allocate(8, 1) != nullptr);
	CHECK_EQ(arena.block_count(), 1u);
}

TEST(arena_large_allocation_starts_new_block)
{
	Arena arena{ 64 };
	auto* small = static_cast<char*>(arena.allocate(8, 1));
	REQUIRE(small != nullptr);
	std::memset(small, 'x', 8);

	auto* large = static_cast<char*>(arena.allocate(1024, 1));
	REQUIRE(large != nullptr);
	CHECK(arena.block_count() >= 2u);
	CHECK(arena.capacity() >= 1024u);
	CHECK_EQ(small[0], 'x');
	CHECK_EQ(small[7], 'x');
}

TEST(arena_single_allocation_larger_than_block_size)
{
	Arena arena{ 64 };
	void* memory{ arena.allocate(1024, 64) };

	REQUIRE(memory != nullptr);
	CHECK_EQ(reinterpret_cast<std::uintptr_t>(memory) % 64, 0u);
	CHECK_EQ(arena.block_count(), 1u);
	CHECK(arena.capacity() >= 1024u);
}

TEST(arena_marker_restore_reuses_memory)
{
	Arena arena{ 256 };
	auto* first = arena.create<int>(1);
	const Arena::Marker marker{ arena.mark() };
	const std::size_t usedAtMarker{ arena.used() };

	auto* second = arena.create<int>(2);
	CHECK(arena.used() > usedAtMarker);

	arena.restore(marker);
	CHECK_EQ(arena.used(), usedAtMarker);

	auto* third = arena.create<int>(3);
	CHECK_EQ(reinterpret_cast<std::uintptr_t>(third),
		reinterpret_cast<std::uintptr_t>(second));
	CHECK_EQ(*first, 1);
	CHECK_EQ(*third, 3);
}

TEST(arena_marker_across_blocks)
{
	Arena arena{ 64 };
	REQUIRE(arena.allocate(64, 1) != nullptr);
	REQUIRE(arena.allocate(64, 1) != nullptr);

	const Arena::Marker marker{ arena.mark() };
	const std::size_t usedAtMarker{ arena.used() };
	const std::size_t blocksAtMarker{ arena.block_count() };

	REQUIRE(arena.allocate(64, 1) != nullptr);
	const std::size_t blocksAfterAllocation{ arena.block_count() };
	CHECK(blocksAfterAllocation > blocksAtMarker);

	arena.restore(marker);

	CHECK_EQ(arena.used(), usedAtMarker);
	CHECK_EQ(arena.block_count(), blocksAfterAllocation);

	REQUIRE(arena.allocate(64, 1) != nullptr);
	CHECK_EQ(arena.used(), usedAtMarker + 64);
}

TEST(arena_nested_scopes_restore_in_order)
{
	Arena arena{ 512 };
	auto* outer = arena.create<int>(1);
	const std::size_t usedOuter{ arena.used() };

	{
		Arena::Scope scope{ arena };
		auto* inner = arena.create<int>(2);
		const std::size_t usedInner{ arena.used() };

		{
			Arena::Scope nested{ arena };
			auto* deepest = arena.create<int>(3);
			CHECK_EQ(*deepest, 3);
		}

		CHECK_EQ(arena.used(), usedInner);
		CHECK_EQ(*inner, 2);
	}

	CHECK_EQ(arena.used(), usedOuter);
	CHECK_EQ(*outer, 1);
}

TEST(arena_scope_can_end_early_and_move)
{
	Arena arena{ 256 };
	const std::size_t usedStart{ arena.used() };

	{
		Arena::Scope scope{ arena };
		(void)arena.create<int>(1);
		CHECK(arena.used() > usedStart);
		scope.end();
		CHECK_EQ(arena.used(), usedStart);
	}

	CHECK_EQ(arena.used(), usedStart);

	{
		Arena::Scope outer{ arena };
		(void)arena.create<int>(3);
		const std::size_t usedInside{ arena.used() };

		{
			Arena::Scope moved{ std::move(outer) };
			CHECK_EQ(arena.used(), usedInside);
		}

		CHECK_EQ(arena.used(), usedStart);
	}

	CHECK_EQ(arena.used(), usedStart);
}

TEST(arena_create_constructs_and_destroy_destructs)
{
	int alive{ 0 };
	Arena arena;

	Tracked* object{ arena.create<Tracked>(&alive) };
	REQUIRE(object != nullptr);
	CHECK_EQ(alive, 1);

	arena.destroy(object);
	CHECK_EQ(alive, 0);
}

TEST(arena_reset_does_not_destroy_objects)
{
	int alive{ 0 };
	Arena arena;

	Tracked* object{ arena.create<Tracked>(&alive) };
	REQUIRE(object != nullptr);
	CHECK_EQ(alive, 1);

	arena.reset();
	CHECK_EQ(alive, 1);

	arena.destroy(object);
	CHECK_EQ(alive, 0);
}

TEST(arena_throwing_constructor_rolls_back)
{
	Arena arena{ 64 };
	(void)arena.create<int>(1);
	const std::size_t usedBefore{ arena.used() };
	const std::size_t blocksBefore{ arena.block_count() };

	bool threw{ false };
	try
	{
		(void)arena.create<Thrower>(true);
	}
	catch (const std::runtime_error&)
	{
		threw = true;
	}

	CHECK(threw);
	CHECK_EQ(arena.used(), usedBefore);
	CHECK_EQ(arena.block_count(), blocksBefore);
}

TEST(arena_supports_over_aligned_types)
{
	Arena arena{ 256 };
	AlignedValue* value{ arena.create<AlignedValue>(42) };

	REQUIRE(value != nullptr);
	CHECK_EQ(reinterpret_cast<std::uintptr_t>(value) % alignof(AlignedValue), 0u);
	CHECK_EQ(value->value, 42);
}

TEST(arena_move_preserves_allocations)
{
	Arena arena{ 128 };
	int* value{ arena.create<int>(5) };
	const std::size_t used{ arena.used() };
	const std::size_t capacity{ arena.capacity() };

	Arena moved{ std::move(arena) };

	CHECK_EQ(*value, 5);
	CHECK_EQ(moved.used(), used);
	CHECK_EQ(moved.capacity(), capacity);
	CHECK_EQ(moved.block_count(), 1u);

	CHECK(arena.empty());
	CHECK_EQ(arena.capacity(), 0u);
	CHECK_EQ(arena.block_count(), 0u);

	int* other{ arena.create<int>(6) };
	REQUIRE(other != nullptr);
	CHECK_EQ(*other, 6);
}

TEST(arena_move_assignment_preserves_allocations)
{
	Arena source{ 128 };
	int* value{ source.create<int>(7) };
	const std::size_t used{ source.used() };

	Arena target{ 64 };
	(void)target.allocate(8, 1);

	target = std::move(source);

	CHECK_EQ(*value, 7);
	CHECK_EQ(target.used(), used);
	CHECK(target.capacity() >= used);
}
