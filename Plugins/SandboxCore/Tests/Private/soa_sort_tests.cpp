#include <SandboxCore/soa_permutation.h>
#include <SandboxCore/soa_vectors.h>
#include <SandboxCore/tick_countdown.h>

#include "TestHarness.h"

namespace {
void add_row(FVectors3f& vectors, int32 const id) {
    vectors.add(static_cast<float>(id), static_cast<float>(id + 100), static_cast<float>(id + 200));
}

void check_row_associations(FVectors3f const& vectors) {
    auto const n{vectors.num()};
    for (int32 i{}; i < n; ++i) {
        auto const id{static_cast<int32>(vectors.xs[i])};
        CHECK(vectors.ys[i] == static_cast<float>(id + 100));
        CHECK(vectors.zs[i] == static_cast<float>(id + 200));
    }
}
}

TEST_CASE("SandboxCore.SoaSort.Vectors3f.SortPreservesParallelStreams") {
    FVectors3f vectors;
    add_row(vectors, 30);
    add_row(vectors, 10);
    add_row(vectors, 20);
    add_row(vectors, 40);

    TArray<int32> scratch_indices;
    scratch_indices.SetNumUninitialized(6);
    scratch_indices[4] = 1234;
    scratch_indices[5] = 5678;

    vectors.sort([](auto const& soa, int32 const lhs, int32 const rhs) { return soa.xs[lhs] < soa.xs[rhs]; }, scratch_indices);

    check_row_associations(vectors);
    CHECK((vectors.xs == TArray<float>{10.f, 20.f, 30.f, 40.f}));
    CHECK(scratch_indices[4] == 1234);
    CHECK(scratch_indices[5] == 5678);
}

TEST_CASE("SandboxCore.SoaSort.Vectors3f.CustomComparatorAndEquivalentKeys") {
    FVectors3f vectors;
    add_row(vectors, 30);
    add_row(vectors, 10);
    add_row(vectors, 20);
    add_row(vectors, 40);

    TArray<int32> scratch_indices;
    scratch_indices.SetNumUninitialized(vectors.num());
    vectors.sort(
        [](auto const& soa, int32 const lhs, int32 const rhs) {
            auto const lhs_key{static_cast<int32>(soa.xs[lhs]) % 20};
            auto const rhs_key{static_cast<int32>(soa.xs[rhs]) % 20};
            if (lhs_key != rhs_key) {
                return lhs_key < rhs_key;
            }
            return soa.zs[lhs] > soa.zs[rhs];
        },
        scratch_indices);

    check_row_associations(vectors);
    for (int32 i{1}; i < vectors.num(); ++i) {
        auto const previous_key{static_cast<int32>(vectors.xs[i - 1]) % 20};
        auto const current_key{static_cast<int32>(vectors.xs[i]) % 20};
        CHECK(previous_key <= current_key);
        if (previous_key == current_key) {
            CHECK(vectors.zs[i - 1] > vectors.zs[i]);
        }
    }
}

TEST_CASE("SandboxCore.SoaSort.Vectors3f.EmptySingleAndSortedInputs") {
    TArray<int32> empty_scratch;
    FVectors3f empty;
    empty.sort([](auto const& soa, int32 const lhs, int32 const rhs) { return soa.xs[lhs] < soa.xs[rhs]; }, empty_scratch);
    CHECK(empty.is_empty());

    FVectors3f single;
    add_row(single, 42);
    TArray<int32> single_scratch{0, 99};
    single.sort([](auto const& soa, int32 const lhs, int32 const rhs) { return soa.xs[lhs] < soa.xs[rhs]; }, single_scratch);
    check_row_associations(single);
    CHECK(single.xs[0] == 42.f);
    CHECK(single_scratch[1] == 99);

    FVectors3f already_sorted;
    add_row(already_sorted, 10);
    add_row(already_sorted, 20);
    add_row(already_sorted, 30);
    FVectors3f reverse_sorted;
    add_row(reverse_sorted, 30);
    add_row(reverse_sorted, 20);
    add_row(reverse_sorted, 10);
    TArray<int32> scratch_indices{0, 0, 0};
    auto const by_x = [](auto const& soa, int32 const lhs, int32 const rhs) { return soa.xs[lhs] < soa.xs[rhs]; };
    already_sorted.sort(by_x, scratch_indices);
    reverse_sorted.sort(by_x, scratch_indices);

    check_row_associations(already_sorted);
    check_row_associations(reverse_sorted);
    CHECK((already_sorted.xs == TArray<float>{10.f, 20.f, 30.f}));
    CHECK((reverse_sorted.xs == TArray<float>{10.f, 20.f, 30.f}));
}

TEST_CASE("SandboxCore.SoaSort.TickCountdown.PermutesCounters") {
    FTickCountdown8 countdown{3, 0};
    countdown.set_counter(0, 1);
    countdown.set_counter(1, 2);
    countdown.set_counter(2, 3);
    TArray<int32> indices{2, 0, 1};

    ml::apply_permutation(countdown, indices);

    CHECK(countdown.counters()[0] == 3);
    CHECK(countdown.counters()[1] == 1);
    CHECK(countdown.counters()[2] == 2);
    CHECK((indices == TArray<int32>{2, 0, 1}));
}
