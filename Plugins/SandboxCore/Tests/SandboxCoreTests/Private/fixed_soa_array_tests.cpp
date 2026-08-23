#include "fixed_soa_test_types.h"

#include <SandboxCore/soa_vectors_3f.h>

#include "CoreMinimal.h"
#include "TestHarness.h"

#include <type_traits>

namespace {
template <typename T>
concept HasCapacityView = requires(T& value) { value.capacity_view(); };
}

TEST_CASE("SandboxCore.FixedSoA.Trivial vector storage exposes active and capacity views") {
    TFixedVectors3f<4> values{};

    static_assert(TFixedVectors3f<4>::capacity() == 4);
    static_assert(TFixedVectors3f<0>::capacity() == 0);
    static_assert(HasCapacityView<TFixedVectors3f<4>>);

    auto capacity_view{values.capacity_view()};
    capacity_view.xs[0] = 1.f;
    capacity_view.ys[0] = 2.f;
    capacity_view.zs[0] = 3.f;
    capacity_view.xs[1] = 4.f;
    capacity_view.ys[1] = 5.f;
    capacity_view.zs[1] = 6.f;
    values.set_num_uninitialised(2);

    CHECK(values.num() == 2);
    CHECK(values[0] == FVector3f{1.f, 2.f, 3.f});
    CHECK(values[1] == FVector3f{4.f, 5.f, 6.f});
    CHECK(values.get_const_view().xs.Num() == values.num());
    CHECK(values.get_const_view().ys.Num() == values.num());
    CHECK(values.get_const_view().zs.Num() == values.num());

    values.add(7.f, 8.f, 9.f);
    CHECK(values.at(2) == FVector3f{7.f, 8.f, 9.f});
}

TEST_CASE("SandboxCore.FixedSoA.Recursive non-trivial rows share one logical size") {
    using Rows = ml::fixed_soa_tests::TTestFixedRowsArray<4>;
    static_assert(!HasCapacityView<Rows>);

    Rows rows{};
    auto first_reference{MakeShared<int32>(10)};
    auto second_reference{MakeShared<int32>(20)};

    CHECK(rows.emplace_back(FString{TEXT("first")}, first_reference, 100) == 0);
    CHECK(rows.add(FString{TEXT("second")}, second_reference, 200) == 1);

    auto const view{rows.get_const_view()};
    CHECK(rows.num() == 2);
    CHECK(view.children.num() == rows.num());
    CHECK(view.children.names.Num() == rows.num());
    CHECK(view.children.references.Num() == rows.num());
    CHECK(view.ids.Num() == rows.num());
    CHECK(view.children.names[0] == TEXT("first"));
    CHECK(*view.children.references[1] == 20);
    CHECK(view.ids[1] == 200);
}

TEST_CASE("SandboxCore.FixedSoA.Pop resize and reset destroy non-trivial leaves") {
    using Rows = ml::fixed_soa_tests::TTestFixedRowsArray<3>;

    Rows rows{};
    TSharedPtr<int32> reference{MakeShared<int32>(42)};
    TWeakPtr<int32> weak_reference{reference};
    rows.add(FString{TEXT("tracked")}, reference, 1);
    reference.Reset();

    CHECK(weak_reference.IsValid());
    rows.pop();
    CHECK(!weak_reference.IsValid());

    rows.set_num(3);
    CHECK(rows.num() == 3);
    rows.set_num(1);
    CHECK(rows.num() == 1);
    rows.reset();
    CHECK(rows.is_empty());
}

TEST_CASE("SandboxCore.FixedSoA.Copy move append and remove-at-swap preserve rows") {
    using Rows = ml::fixed_soa_tests::TTestFixedRowsArray<4>;
    using AlternateRows = ml::fixed_soa_tests::TTestFixedRowsArrayAlternate<4>;

    Rows source{};
    source.add(FString{TEXT("first")}, MakeShared<int32>(10), 100);
    source.add(FString{TEXT("second")}, MakeShared<int32>(20), 200);
    source.add(FString{TEXT("third")}, MakeShared<int32>(30), 300);

    Rows copied{source};
    Rows moved{MoveTemp(copied)};
    CHECK(copied.is_empty());
    CHECK(moved.num() == 3);
    CHECK(moved.get_const_view().children.names[1] == TEXT("second"));

    moved.remove_at_swap(0, 1, EAllowShrinking::No);
    auto const moved_view{moved.get_const_view()};
    CHECK(moved.num() == 2);
    CHECK(moved_view.children.names[0] == TEXT("third"));
    CHECK(moved_view.ids[0] == 300);
    CHECK(moved_view.children.names[1] == TEXT("second"));

    AlternateRows alternate{};
    alternate.append_from(moved);
    CHECK(alternate.num() == moved.num());
    CHECK(alternate.get_const_view().children.names[0] == TEXT("third"));
    CHECK(alternate.get_const_view().ids[1] == 200);

    moved.copy_element(1, source, 0);
    CHECK(moved.get_const_view().children.names[1] == TEXT("first"));
    CHECK(moved.get_const_view().ids[1] == 100);
}
