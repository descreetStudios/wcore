#include <wcore/test.hpp>
#include <wcore/handle.hpp>

#include <format>
#include <string>
#include <utility>

using namespace wcore;
using namespace test;

TEST(handle_default_is_invalid)
{
	Handle<int> h;
	CHECK(!h.valid());
	CHECK(!static_cast<bool>(h));
	CHECK_EQ(h.index(), Handle<int>::InvalidIndex);
	CHECK_EQ(h.generation(), 0u);
}

TEST(handle_equality_uses_index_and_generation)
{
	const Handle<int> a{ 1, 0 };
	const Handle<int> b{ 1, 0 };
	const Handle<int> c{ 2, 0 };
	const Handle<int> d{ 1, 1 };

	CHECK(a == b);
	CHECK(a != c);
	CHECK(a != d);
	CHECK_EQ(a.raw(), (u64{ 1 } << 32));
}

TEST(handle_formats)
{
	CHECK_EQ(std::format("{}", Handle<int>{ 2, 3 }), std::string("Handle(2:3)"));
	CHECK_EQ(std::format("{}", Handle<int>{}), std::string("Handle(invalid)"));
}

TEST(handle_pool_create_returns_valid_handle)
{
	HandlePool<int> pool;
	const Handle<int> h{ pool.create(42) };

	CHECK(h.valid());
	CHECK_EQ(h.index(), 0u);
	CHECK_EQ(h.generation(), 0u);
	CHECK(pool.alive(h));
	REQUIRE(pool.get(h) != nullptr);
	CHECK_EQ(*pool.get(h), 42);
	CHECK_EQ(pool.size(), 1u);
	CHECK(!pool.empty());
}

TEST(handle_pool_creates_distinct_slots)
{
	HandlePool<std::string> pool;
	const Handle<std::string> a{ pool.create("one") };
	const Handle<std::string> b{ pool.create("two") };
	const Handle<std::string> c{ pool.create("three") };

	CHECK_EQ(a.index(), 0u);
	CHECK_EQ(b.index(), 1u);
	CHECK_EQ(c.index(), 2u);
	CHECK_EQ(pool.size(), 3u);
	CHECK_EQ(*pool.get(b), std::string("two"));
}

TEST(handle_pool_destroy_invalidates_handle)
{
	HandlePool<int> pool;
	const Handle<int> h{ pool.create(7) };

	CHECK(pool.destroy(h));
	CHECK(!pool.alive(h));
	CHECK(pool.get(h) == nullptr);
	CHECK_EQ(pool.size(), 0u);
	CHECK(pool.empty());

	CHECK(!pool.destroy(h));
	CHECK(!pool.destroy(Handle<int>{}));
}

TEST(handle_pool_reuses_slot_with_new_generation)
{
	HandlePool<int> pool;
	const Handle<int> old{ pool.create(1) };

	CHECK(pool.destroy(old));
	const Handle<int> fresh{ pool.create(2) };

	CHECK_EQ(fresh.index(), old.index());
	CHECK_EQ(fresh.generation(), old.generation() + 1);
	CHECK(!pool.alive(old));
	CHECK(pool.get(old) == nullptr);
	CHECK_EQ(*pool.get(fresh), 2);
}

TEST(handle_pool_destroy_keeps_other_handles_valid)
{
	HandlePool<int> pool;
	const Handle<int> a{ pool.create(10) };
	const Handle<int> b{ pool.create(20) };
	const Handle<int> c{ pool.create(30) };

	CHECK(pool.destroy(a));
	CHECK_EQ(*pool.get(b), 20);
	CHECK_EQ(*pool.get(c), 30);
	CHECK_EQ(pool.size(), 2u);

	CHECK(pool.destroy(c));
	CHECK_EQ(*pool.get(b), 20);
	CHECK_EQ(pool.size(), 1u);
}

TEST(handle_pool_clear_invalidates_existing_handles)
{
	HandlePool<int> pool;
	const Handle<int> a{ pool.create(1) };
	const Handle<int> b{ pool.create(2) };

	pool.clear();
	CHECK_EQ(pool.size(), 0u);
	CHECK(pool.empty());
	CHECK(!pool.alive(a));
	CHECK(!pool.alive(b));

	const Handle<int> c{ pool.create(3) };
	CHECK(c.index() == a.index() || c.index() == b.index());
	CHECK(!pool.alive(a));
	CHECK(!pool.alive(b));
	CHECK(pool.alive(c));
}

TEST(handle_pool_supports_move_only_types)
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

	HandlePool<MoveOnly> pool;
	const Handle<MoveOnly> h{ pool.create(5) };

	REQUIRE(pool.get(h) != nullptr);
	CHECK_EQ(pool.get(h)->value, 5);
}

TEST(handle_pool_destroys_values)
{
	struct Tracked
	{
		int* count;

		explicit Tracked(int* c) : count(c) { ++*count; }
		Tracked(const Tracked&) = delete;
		Tracked& operator=(const Tracked&) = delete;

		Tracked(Tracked&& other) noexcept : count(other.count) { other.count = nullptr; }

		Tracked& operator=(Tracked&& other) noexcept
		{
			if (this != &other)
			{
				release();
				count = other.count;
				other.count = nullptr;
			}
			return *this;
		}

		~Tracked() { release(); }

		void release()
		{
			if (count)
			{
				--*count;
				count = nullptr;
			}
		}
	};

	int count{ 0 };
	{
		HandlePool<Tracked> pool;
		const Handle<Tracked> a{ pool.create(&count) };
		const Handle<Tracked> b{ pool.create(&count) };
		CHECK_EQ(count, 2);

		pool.destroy(a);
		CHECK_EQ(count, 1);

		pool.destroy(b);
		CHECK_EQ(count, 0);

		pool.create(&count);
		pool.create(&count);
		CHECK_EQ(count, 2);
	}
	CHECK_EQ(count, 0);
}

TEST(handle_pool_for_each_visits_live_values)
{
	HandlePool<int> pool;
	const Handle<int> a{ pool.create(1) };
	const Handle<int> b{ pool.create(2) };
	const Handle<int> c{ pool.create(3) };

	int sum{ 0 };
	pool.for_each([&sum](int& value) { sum += value; });
	CHECK_EQ(sum, 6);

	CHECK(pool.destroy(b));
	sum = 0;
	pool.for_each([&sum](int& value) { sum += value; });
	CHECK_EQ(sum, 4);

	pool.for_each_with_handle([&](Handle<int> handle, int& value)
	{
		CHECK(pool.alive(handle));
		if (value == 1) CHECK(handle == a);
		if (value == 3) CHECK(handle == c);
	});
}

TEST(handle_pool_const_access)
{
	HandlePool<int> pool;
	const Handle<int> h{ pool.create(9) };
	const HandlePool<int>& constPool{ pool };

	CHECK(constPool.alive(h));
	REQUIRE(constPool.get(h) != nullptr);
	CHECK_EQ(*constPool.get(h), 9);
	CHECK_EQ(constPool[h], 9);

	int sum{ 0 };
	constPool.for_each([&sum](const int& value) { sum += value; });
	CHECK_EQ(sum, 9);
}

TEST(handle_pool_reserve_keeps_handles_valid)
{
	HandlePool<int> pool;
	const Handle<int> a{ pool.create(1) };

	pool.reserve(100);
	CHECK(pool.capacity() >= 100u);

	const Handle<int> b{ pool.create(2) };
	CHECK_EQ(*pool.get(a), 1);
	CHECK_EQ(*pool.get(b), 2);
}

TEST(handle_pool_move_preserves_handles)
{
	HandlePool<int> pool;
	const Handle<int> h{ pool.create(5) };

	HandlePool<int> moved{ std::move(pool) };
	CHECK(moved.alive(h));
	CHECK_EQ(*moved.get(h), 5);
}

TEST(handle_hash_is_stable_for_equal_handles)
{
	const std::hash<Handle<int>> hash;
	const Handle<int> a{ 3, 7 };

	CHECK_EQ(hash(a), hash(Handle<int>{ 3, 7 }));
}
