#include "Generated.h"

#include <stdexcept>
#include <utility>

namespace codegen_compile_fixture {

auto FRows::manual_value() const -> int32 {
    return 77;
}

} // namespace codegen_compile_fixture

namespace {

using namespace codegen_compile_fixture;

void test_homogeneous_storage() {
    FValuesf values;
    values.add(3.0f, 30.0f);
    values.add(FVector2f{1.0f, 10.0f});
    values.add(FPoint2f{2.0f, 20.0f});
    values.validate_array_sizes();

    TArray<int32> scratch;
    scratch.AddUninitialized(values.num());
    values.sort([](auto const& rows,
                   int32 const lhs,
                   int32 const rhs) { return rows.xs[lhs] < rows.xs[rhs]; },
                scratch);
    check(values.at(0).X == 1.0f && values.at(0).Y == 10.0f);
    check(values.at(1).X == 2.0f && values.at(1).Y == 20.0f);
    check(values.get_const_view().at(2).X == 3.0f);

    FValuesf copy;
    copy.add_defaulted(values.num());
    copy.copy_elements(0, values.get_const_view(), 0, values.num());
    check(copy.at(1).Y == 20.0f);
    copy.remove_at_swap(0, 1, EAllowShrinking::No);
    check(copy.num() == 2);
    check(copy.at(0).X == 3.0f && copy.at(0).Y == 30.0f);

    FScalari scalar;
    scalar.values.Add(7);
    check(scalar.get_const_view().values[0] == 7);

    FValuesd doubles;
    doubles.add(4.0, 5.0);
    check(doubles.at(0).X == 4.0 && doubles.at(0).Y == 5.0);
}

void test_dynamic_soa() {
    FRows rows;
    rows.ids.Add(30);
    rows.weights.Add(3.0f);
    rows.ids.Add(10);
    rows.weights.Add(1.0f);
    rows.ids.Add(20);
    rows.weights.Add(2.0f);
    rows.validate_array_sizes();
    rows.reserve(16);
    check(rows.first_id() == 30);
    check(rows.weight_sum() == 4.0f);
    check(rows.manual_value() == 77);

    TArray<int32> scratch;
    scratch.AddUninitialized(rows.num());
    rows.sort([](auto const& values,
                 int32 const lhs,
                 int32 const rhs) { return values.ids[lhs] < values.ids[rhs]; },
              scratch);
    check(rows.at(0).id == 10 && rows.at(0).weight == 1.0f);
    check(rows.at(2).id == 30 && rows.at(2).weight == 3.0f);
    check(rows.slice(1, 1).at(0).id == 20);

    FRows copied;
    copied.add_defaulted(rows.num());
    copied.copy_elements(0, rows.get_const_view(), 0, rows.num());
    check(copied.at(2).weight == 3.0f);
    copied.copy_element(0, rows.get_const_view(), 2);
    check(copied.at(0).id == 30 && copied.at(0).weight == 3.0f);
    copied.copy_to_tail(rows.get_const_view().left(1));
    check(copied.at(2).id == 10);

    FRows appended;
    appended.append_from(rows.get_const_view());
    appended.append_from(copied.get_const_view().left(1));
    check(appended.num() == 4);
    check(appended.at(3).id == 30);
    appended.remove_at_swap(1, 2, EAllowShrinking::No);
    check(appended.num() == 2);
    appended.reset();
    check(appended.is_empty());
    appended.add_uninitialised(2);
    appended.ids[0] = 1;
    appended.ids[1] = 2;
    appended.weights[0] = 10.0f;
    appended.weights[1] = 20.0f;
    appended.set_num(1, EAllowShrinking::No);
    check(appended.num() == 1 && appended.at(0).weight == 10.0f);

    FParents parents;
    parents.keys.Add(1);
    parents.children.values.Add(10);
    parents.keys.Add(2);
    parents.children.values.Add(20);
    parents.remove_at_swap(0, 1, EAllowShrinking::No);
    parents.validate_array_sizes();
    check(parents.keys[0] == 2 && parents.children.values[0] == 20);
}

void test_fixed_soa_lifetimes() {
    check(FTracked::alive == 0);
    {
        TFixedRows<4> rows;
        rows.add(1, FTracked{10});
        rows.add(2, FTracked{20});
        check(rows.num() == 2);
        check(FTracked::alive == 2);
        check(rows.get_const_view().children.tracked[1].value == 20);

        auto copied{rows};
        check(copied.num() == 2);
        check(FTracked::alive == 4);
        auto const* self{&copied};
        copied = *self;
        check(copied.num() == 2 && FTracked::alive == 4);

        auto moved{std::move(copied)};
        check(copied.is_empty());
        check(moved.num() == 2 && FTracked::alive == 4);
        moved.remove_at_swap(0, 1, EAllowShrinking::No);
        check(moved.num() == 1 && FTracked::alive == 3);
        moved.reset();
        check(FTracked::alive == 2);

        bool overflow_rejected{};
        try {
            rows.add_defaulted(3);
        } catch (std::runtime_error const&) {
            overflow_rejected = true;
        }
        check(overflow_rejected);
    }
    check(FTracked::alive == 0);

    TFixedRows<0> empty;
    check(empty.is_empty() && empty.is_full());
    {
        TFixedRowsAlt<1> alternate;
        alternate.add(9, FTracked{90});
        check(alternate.get_const_view().children.tracked[0].value == 90);
    }
    check(FTracked::alive == 0);
}

void test_vectors() {
    FVectors1f scalar;
    scalar.add(FScalar1f{4.0f});
    check(scalar.at(0).X == 4.0f);

    FVectors2f vector2;
    vector2.add(FVector2f{7.0f, 8.0f});
    check(vector2.at(0).X == 7.0f && vector2.at(0).Y == 8.0f);

    FVectors3f vectors;
    vectors.add(3.0f, 30.0f, 300.0f);
    vectors.add(FVector3f{1.0f, 10.0f, 100.0f});
    vectors.add(FVector3f{2.0f, 20.0f, 200.0f});
    TArray<int32> scratch;
    scratch.AddUninitialized(vectors.num());
    vectors.sort([](auto const& rows,
                    int32 const lhs,
                    int32 const rhs) { return rows.xs[lhs] < rows.xs[rhs]; },
                 scratch);
    check(vectors.at(0).X == 1.0f && vectors.at(0).Z == 100.0f);
    check(vectors.at(2).X == 3.0f && vectors.at(2).Y == 30.0f);

    TFixedVectors3<3> fixed;
    fixed.add(1.0f, 2.0f, 3.0f);
    fixed.add(4.0f, 5.0f, 6.0f);
    auto copied{fixed};
    check(copied.at(1).X == 4.0f && copied.at(1).Z == 6.0f);
    copied.remove_at_swap(0, 1, EAllowShrinking::No);
    check(copied.num() == 1 && copied.at(0).X == 4.0f);
}

void test_facades() {
    FTarget target;
    target.value = 5;

    FInlineFacade inline_facade;
    bool unbound_rejected{};
    try {
        static_cast<void>(inline_facade.get());
    } catch (std::runtime_error const&) {
        unbound_rejected = true;
    }
    check(unbound_rejected);
    inline_facade.bind(target);
    check(inline_facade.get() == 5);
    check(inline_facade.get(3) == 8);
    inline_facade.reset();
    check(target.value == 0);

    FSourceFacade source_facade;
    source_facade.bind(target);
    source_facade.set_value(42);
    check(source_facade.get() == 42);
}

void test_enums() {
    static_assert(TEnumTraits<EPlainFixture>::count == 2);
    static_assert(TEnumArray<EPlainFixture, float>::size() == 2);

    TEnumArray<EPlainFixture, float> radii;
    radii[EPlainFixture::First] = 100.0f;
    radii[EPlainFixture::ReadableName] = 200.0f;
    check(radii[EPlainFixture::First] == 100.0f);
    check(radii[EPlainFixture::ReadableName] == 200.0f);

    static_assert(TEnumTraits<EReflectedFixture>::count == 1);
    static_assert(TEnumArray<EReflectedFixture, int32>::size() == 1);
    TEnumArray<EReflectedFixture, int32> reflected_values;
    reflected_values[EReflectedFixture::Visible] = 42;
    check(reflected_values[EReflectedFixture::Visible] == 42);

    auto const& const_radii{radii};
    check(const_radii[EPlainFixture::First] == 100.0f);

    check(std::string_view{LexToString(EPlainFixture::First)} == "First");
    check(to_string_view(EPlainFixture::ReadableName) == "ReadableName");
    check(to_string(EPlainFixture::ReadableName) == "ReadableName");
    check(std::string_view{LexToDisplayString(EPlainFixture::ReadableName)} == "Readable Name");
    check(to_display_string_view(EPlainFixture::ReadableName) == "Readable Name");
    check(to_display_string(EPlainFixture::First) == "First");

    check(std::string_view{LexToString(EReflectedFixture::Visible)} == "Visible");
    check(to_display_string_view(EReflectedFixture::Visible) == "Visible Value");
    check(std::string_view{LexToString(static_cast<EReflectedFixture>(99))} ==
          "<invalid EReflectedFixture>");
}

void test_static_tables() {
    static_assert(FStaticTableFixture::num() == 3);
    static_assert(FStaticTableFixture::first_index == 0);
    static_assert(FStaticTableFixture::third_index == 2);

    FStaticTableFixture values{
        .ids = {10, 20, 30},
        .weights = {1.0f, 2.0f, 3.0f},
    };
    values.apply_arrays([](auto& ids, auto& weights) {
        ids[FStaticTableFixture::second_index] = {};
        weights[FStaticTableFixture::second_index] = {};
    });
    check(values.ids[0] == 10 && values.ids[1] == 0);
    check(values.weights[1] == 0.0f && values.weights[2] == 3.0f);

    FStaticTableFixture other{};
    values.apply_array_pairs(other, [](auto const& source_ids,
                                       auto& destination_ids,
                                       auto const& source_weights,
                                       auto& destination_weights) {
        destination_ids[FStaticTableFixture::third_index] =
            source_ids[FStaticTableFixture::third_index];
        destination_weights[FStaticTableFixture::third_index] =
            source_weights[FStaticTableFixture::third_index];
    });
    check(other.ids[2] == 30 && other.weights[2] == 3.0f);

    auto const& const_values{values};
    int visited{};
    const_values.apply_arrays(
        [&visited]<typename... Columns>(Columns const&...) { visited = sizeof...(Columns); });
    check(visited == 2);
    auto const row{const_values.get_row(FStaticTableFixture::third_index)};
    check(row.id == 30 && row.weight == 3.0f);

    FStaticGroupFixture groups{};
    groups.min_xs[FStaticGroupFixture::second_index] = 1.0f;
    groups.min_ys[FStaticGroupFixture::second_index] = 2.0f;
    groups.min_zs[FStaticGroupFixture::second_index] = 3.0f;
    groups.max_xs[FStaticGroupFixture::second_index] = 4.0f;
    groups.max_ys[FStaticGroupFixture::second_index] = 5.0f;
    groups.max_zs[FStaticGroupFixture::second_index] = 6.0f;

    auto const& const_groups{groups};
    auto const min_point{const_groups.get_min_point(FStaticGroupFixture::second_index)};
    auto const max_point{const_groups.get_max_point(FStaticGroupFixture::second_index)};
    check(min_point.X == 1.0f && min_point.Y == 2.0f && min_point.Z == 3.0f);
    check(max_point.X == 4.0f && max_point.Y == 5.0f && max_point.Z == 6.0f);
}

} // namespace

auto main() -> int {
    try {
        test_homogeneous_storage();
        test_dynamic_soa();
        test_fixed_soa_lifetimes();
        test_vectors();
        test_facades();
        test_enums();
        test_static_tables();
        return 0;
    } catch (std::exception const&) {
        return 1;
    }
}
