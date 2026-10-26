#include <ioj/loop_bounds.h>
#include <ioj/testing/test_case.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>
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

struct RotatedLoopInput {
    std::int32_t begin;
    std::int32_t end;
    std::int32_t offset;
};
struct LoopBoundsResult {
    ioj::FLoopBounds first;
    ioj::FLoopBounds second;
};
using LoopBoundsCase = ioj::testing::TestCase<RotatedLoopInput, LoopBoundsResult>;
using RotatedLoopBounds = testing::TestWithParam<LoopBoundsCase>;

TEST_P(RotatedLoopBounds, ReturnsExpectedBounds) {
    auto const test{GetParam()};
    auto const input{test.input};
    auto const expected{test.expected};
    auto const bounds{ioj::make_rotated_loop_bounds(input.begin, input.end, input.offset)};
    EXPECT_EQ(bounds[0].begin, expected.first.begin);
    EXPECT_EQ(bounds[0].end, expected.first.end);
    EXPECT_EQ(bounds[1].begin, expected.second.begin);
    EXPECT_EQ(bounds[1].end, expected.second.end);
}

constexpr LoopBoundsCase loop_bounds_cases[]{
    {"ZeroOffset", {0, 20, 0}, {{0, 20}, {0, 0}}},
    {"NonzeroBegin", {10, 20, 0}, {{10, 20}, {10, 10}}},
    {"OneStep", {10, 20, 1}, {{11, 20}, {10, 11}}},
    {"HalfRotation", {10, 20, 5}, {{15, 20}, {10, 15}}},
    {"LastStep", {10, 20, 9}, {{19, 20}, {10, 19}}},
    {"FullRotation", {10, 20, 10}, {{10, 20}, {10, 10}}},
    {"WrappedOffset", {10, 20, 13}, {{13, 20}, {10, 13}}},
    {"ZeroBeginRotation", {0, 10, 3}, {{3, 10}, {0, 3}}},
    {"NonzeroBeginRotation", {5, 13, 6}, {{11, 13}, {5, 11}}},
    {"Single", {42, 43, 0}, {{42, 43}, {42, 42}}},
    {"SingleWrapped", {42, 43, 1}, {{42, 43}, {42, 42}}},
    {"Empty", {7, 7, 999}, {{7, 7}, {7, 7}}},
};
INSTANTIATE_TEST_SUITE_P(NativeCoreLoopBounds,
                         RotatedLoopBounds,
                         testing::ValuesIn(loop_bounds_cases),
                         [](auto const& info) { return std::string{info.param.name}; });

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
