#include <wcore/test.hpp>
#include <wcore/assert.hpp>

using namespace wcore;
using namespace test;

static int gCalls{ 0 };

static bool counted_true()
{
	++gCalls;
	return true;
}

static int add(int a, int b)
{
	return a + b;
}

TEST(assert_passes_when_true)
{
	WCORE_ASSERT(true, "should never fire");
	WCORE_ASSERT(1 + 1 == 2, "basic math");
	CHECK(true);
}

TEST(assert_accepts_runtime_expressions)
{
	int x{ 5 };
	std::string s{ "wcore" };
	WCORE_ASSERT(x > 0 && x < 10, "x in range");
	WCORE_ASSERT(!s.empty(), "string not empty");
	WCORE_ASSERT(add(2, 3) == 5, "function call in expression");
	CHECK_EQ(x, 5);
}

TEST(assert_evaluates_expression_exactly_once)
{
	gCalls = 0;
	WCORE_ASSERT(counted_true(), "side effect check");
	CHECK_EQ(gCalls, 1);
}

TEST(assert_is_a_single_statement)
{
	int branch{ 0 };
	if (branch == 0)
		WCORE_ASSERT(true, "if-branch");
	else
		branch = 2;
	CHECK_EQ(branch, 0);

	if (branch == 1)
		branch = 3;
	else
		WCORE_ASSERT(true, "else-branch");
	CHECK_EQ(branch, 0);
}

TEST(assert_debug_passes_when_true)
{
	WCORE_ASSERT_DEBUG(true, "should never fire");
	WCORE_ASSERT_DEBUG(add(1, 1) == 2, "math");
	CHECK(true);
}

TEST(assert_debug_evaluation_depends_on_ndebug)
{
	gCalls = 0;
	WCORE_ASSERT_DEBUG(counted_true(), "side effect check");
#ifdef NDEBUG
	// Expression must not be evaluated.
	CHECK_EQ(gCalls, 0);
#else
	// Evaluated exactly once.
	CHECK_EQ(gCalls, 1);
#endif
}

TEST(assert_debug_is_a_single_statement)
{
	int branch{ 0 };
	if (branch == 0)
		WCORE_ASSERT_DEBUG(true, "if-branch");
	else
		branch = 2;
	CHECK_EQ(branch, 0);
}

TEST(assert_unreachable_compiles_in_dead_branch)
{
	volatile bool never{ false };
	if (never)
		WCORE_UNREACHABLE("must never run");
	CHECK(!never);
}