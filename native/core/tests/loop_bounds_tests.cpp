#include <sandbox/core/loop_bounds.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <vector>

namespace {
void expect_bounds(std::int32_t const begin,
                   std::int32_t const end,
                   std::int32_t const offset,
                   ml::FLoopBounds const first,
                   ml::FLoopBounds const second) {
    auto const bounds{ml::make_rotated_loop_bounds(begin, end, offset)};
    EXPECT_EQ(bounds[0].begin, first.begin);
    EXPECT_EQ(bounds[0].end, first.end);
    EXPECT_EQ(bounds[1].begin, second.begin);
    EXPECT_EQ(bounds[1].end, second.end);
}

void expect_sequence(std::int32_t const begin, std::int32_t const end, std::int32_t const offset) {
    std::vector<std::int32_t> actual;
    for (auto const bound : ml::make_rotated_loop_bounds(begin, end, offset)) {
        for (auto i{bound.begin}; i < bound.end; ++i) {
            actual.push_back(i);
        }
    }

    std::vector<std::int32_t> expected;
    auto const count{end - begin};
    if (count > 0) {
        auto const normalized_offset{offset % count};
        for (std::int32_t i{}; i < count; ++i) {
            expected.push_back(begin + ((i + normalized_offset) % count));
        }
    }
    EXPECT_EQ(actual, expected);
}
}

TEST(NativeCoreLoopBounds, ReturnsExpectedBounds) {
    expect_bounds(0, 20, 0, {0, 20}, {0, 0});
    expect_bounds(10, 20, 0, {10, 20}, {10, 10});
    expect_bounds(10, 20, 1, {11, 20}, {10, 11});
    expect_bounds(10, 20, 5, {15, 20}, {10, 15});
    expect_bounds(10, 20, 9, {19, 20}, {10, 19});
    expect_bounds(10, 20, 10, {10, 20}, {10, 10});
    expect_bounds(10, 20, 13, {13, 20}, {10, 13});
    expect_bounds(0, 10, 3, {3, 10}, {0, 3});
    expect_bounds(5, 13, 6, {11, 13}, {5, 11});
    expect_bounds(42, 43, 0, {42, 43}, {42, 42});
    expect_bounds(42, 43, 1, {42, 43}, {42, 42});
    expect_bounds(7, 7, 999, {7, 7}, {7, 7});
}

TEST(NativeCoreLoopBounds, MatchesModuloReference) {
    for (std::int32_t offset{}; offset < 24; ++offset) {
        expect_sequence(10, 20, offset);
    }
    for (std::int32_t offset{}; offset < 16; ++offset) {
        expect_sequence(0, 10, offset);
        expect_sequence(5, 13, offset);
    }
    expect_sequence(100, 300, std::numeric_limits<std::int32_t>::max());
    expect_sequence(42, 43, std::numeric_limits<std::int32_t>::max());
    expect_sequence(7, 7, 0);
}
