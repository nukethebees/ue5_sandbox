#include <sandbox/core/invoke.h>

#include <gtest/gtest.h>

namespace {
auto sum{0};

void add_to_sum(int const value) {
    sum += value;
}
}

TEST(InvokeOnAll, InvokesCompileTimeCallableForEveryValue) {
    sum = 0;

    ml::invoke_on_all<add_to_sum>(1, 2, 3);

    EXPECT_EQ(sum, 6);
}

TEST(InvokeOnAll, InvokesRuntimeCallableForEveryValue) {
    auto result{0};

    ml::invoke_on_all([&result](int const value) { result += value; }, 2, 4, 8);

    EXPECT_EQ(result, 14);
}
