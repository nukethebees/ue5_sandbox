#include <SandboxCore/loop_bounds.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

#include <limits>

namespace {
auto make_reference_sequence(int32 const begin, int32 const end, int32 const offset) -> TArray<int32> {
    TArray<int32> result;
    int32 const count{end - begin};
    result.Reserve(count);
    if (count == 0) {
        return result;
    }

    int32 const normalized_offset{offset % count};

    for (int32 i{}; i < count; ++i) {
        result.Add(begin + ((i + normalized_offset) % count));
    }

    return result;
}

void check_bounds(TStaticArray<ml::FLoopBounds, 2> const got, ml::FLoopBounds const expected_first, ml::FLoopBounds const expected_second) {
    CHECK(got[0].begin == expected_first.begin);
    CHECK(got[0].end == expected_first.end);
    CHECK(got[1].begin == expected_second.begin);
    CHECK(got[1].end == expected_second.end);
}

void check_sequence(int32 const begin, int32 const end, int32 const offset) {
    auto const bounds{ml::make_rotated_loop_bounds(begin, end, offset)};
    TArray<int32> actual;
    for (auto const bound : bounds) {
        for (int32 i{bound.begin}; i < bound.end; ++i) {
            actual.Add(i);
        }
    }

    auto const expected{make_reference_sequence(begin, end, offset)};
    CHECK(actual == expected);

    auto const count{end - begin};
    TArray<int32> visits;
    visits.Init(0, count);
    for (auto const value : actual) {
        CHECK(value >= begin);
        CHECK(value < end);
        if ((value >= begin) && (value < end)) {
            ++visits[value - begin];
        }
    }
    for (int32 const visit_count : visits) {
        CHECK(visit_count == 1);
    }
}
}

TEST_CASE("SandboxCore.LoopBounds.ReturnsExpectedBounds") {
    check_bounds(ml::make_rotated_loop_bounds(0, 20, 0), {0, 20}, {0, 0});
    check_bounds(ml::make_rotated_loop_bounds(10, 20, 0), {10, 20}, {10, 10});
    check_bounds(ml::make_rotated_loop_bounds(10, 20, 1), {11, 20}, {10, 11});
    check_bounds(ml::make_rotated_loop_bounds(10, 20, 5), {15, 20}, {10, 15});
    check_bounds(ml::make_rotated_loop_bounds(10, 20, 9), {19, 20}, {10, 19});
    check_bounds(ml::make_rotated_loop_bounds(10, 20, 10), {10, 20}, {10, 10});
    check_bounds(ml::make_rotated_loop_bounds(10, 20, 13), {13, 20}, {10, 13});
    check_bounds(ml::make_rotated_loop_bounds(0, 10, 3), {3, 10}, {0, 3});
    check_bounds(ml::make_rotated_loop_bounds(5, 13, 6), {11, 13}, {5, 11});
    check_bounds(ml::make_rotated_loop_bounds(42, 43, 0), {42, 43}, {42, 42});
    check_bounds(ml::make_rotated_loop_bounds(42, 43, 1), {42, 43}, {42, 42});
    check_bounds(ml::make_rotated_loop_bounds(7, 7, 999), {7, 7}, {7, 7});
}

TEST_CASE("SandboxCore.LoopBounds.MatchesModuloReference") {
    for (int32 offset{}; offset < 24; ++offset) {
        check_sequence(10, 20, offset);
    }
    for (int32 offset{}; offset < 16; ++offset) {
        check_sequence(0, 10, offset);
    }
    for (int32 offset{}; offset < 16; ++offset) {
        check_sequence(5, 13, offset);
    }

    check_sequence(100, 300, std::numeric_limits<int32>::max());
    check_sequence(42, 43, std::numeric_limits<int32>::max());
    check_sequence(7, 7, 0);
}
