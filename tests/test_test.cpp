#include <wcore/test.hpp>

using namespace wcore;
using namespace test;

TEST(addition)
{
    CHECK_EQ(1 + 1, 2);
    CHECK(2 > 1);
}

TEST(vector_not_empty)
{
    std::vector<int> v{ 1, 2, 3 };
    REQUIRE(!v.empty());
    CHECK_EQ(v.size(), 3u);
}