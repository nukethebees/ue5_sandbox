#include <ioj/loop_bounds.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <vector>

namespace {
void expect_sequence(std::int32_t const begin, std::int32_t const end, std::int32_t const offset) {
    std::vector<std::int32_t> actual;
    for (auto const bound : ioj::make_rotated_loop_bounds(begin, end, offset)) {
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

struct LoopBoundsCase {
    std::int32_t begin;
    std::int32_t end;
    std::int32_t offset;
    ioj::FLoopBounds first;
    ioj::FLoopBounds second;
};
using RotatedLoopBounds = testing::TestWithParam<LoopBoundsCase>;

TEST_P(RotatedLoopBounds, ReturnsExpectedBounds) {
    auto const test{GetParam()};
    auto const bounds{ioj::make_rotated_loop_bounds(test.begin, test.end, test.offset)};
    EXPECT_EQ(bounds[0].begin, test.first.begin);
    EXPECT_EQ(bounds[0].end, test.first.end);
    EXPECT_EQ(bounds[1].begin, test.second.begin);
    EXPECT_EQ(bounds[1].end, test.second.end);
}

constexpr LoopBoundsCase loop_bounds_cases[]{
    {0, 20, 0, {0, 20}, {0, 0}},
    {10, 20, 0, {10, 20}, {10, 10}},
    {10, 20, 1, {11, 20}, {10, 11}},
    {10, 20, 5, {15, 20}, {10, 15}},
    {10, 20, 9, {19, 20}, {10, 19}},
    {10, 20, 10, {10, 20}, {10, 10}},
    {10, 20, 13, {13, 20}, {10, 13}},
    {0, 10, 3, {3, 10}, {0, 3}},
    {5, 13, 6, {11, 13}, {5, 11}},
    {42, 43, 0, {42, 43}, {42, 42}},
    {42, 43, 1, {42, 43}, {42, 42}},
    {7, 7, 999, {7, 7}, {7, 7}},
};
INSTANTIATE_TEST_SUITE_P(NativeCoreLoopBounds,
                         RotatedLoopBounds,
                         testing::ValuesIn(loop_bounds_cases));

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
