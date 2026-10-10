#include <wcore/test.hpp>

using namespace wcore;
using namespace test;

TEST(Addition)
{
    CHECK_EQ(1 + 1, 2);
    CHECK(2 > 1);
}

TEST(VectorNotEmpty)
{
    std::vector<int> v{ 1, 2, 3 };
    REQUIRE(!v.empty());
    CHECK_EQ(v.size(), 3u);
}