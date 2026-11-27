#include <ioj/members.h>

#include <gtest/gtest.h>

namespace {
struct MixedMembers {
    int first;
    double other;
    int& last;
};

TEST(NativeCoreMembers, ComparesOnlyExactMatches) {
    int last{3};
    MixedMembers const instance{3, 7.5, last};
    EXPECT_TRUE(ioj::members_equal(3, instance));

    last = 4;
    EXPECT_FALSE(ioj::members_equal(3, instance));
    EXPECT_FALSE(ioj::members_equal(4, instance));
}

TEST(NativeCoreMembers, AssignsEveryExactMatchAtCompileTime) {
    constexpr auto assigned{[] {
        int last{2};
        MixedMembers instance{1, 7.5, last};
        ioj::assign_members(4, instance);
        return ioj::members_equal(4, instance) && instance.other == 7.5 && last == 4;
    }()};
    static_assert(assigned);
    EXPECT_TRUE(assigned);
}
}
