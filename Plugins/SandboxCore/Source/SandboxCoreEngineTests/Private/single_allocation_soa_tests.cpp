#include "generated/soa_fixture.h"

#include <SandboxCore/single_allocation/runtime.h>

#include <CQTest.h>

#include <cstdint>
#include <type_traits>
#include <utility>

TEST_CLASS(SingleAllocationSoa, "SandboxCoreEngine.UnitTests")
{
    using Owner = ml::soa_test_fixture::SingleRows;

    TEST_METHOD(CompactSelfCopyPreservesEveryColumn)
    {
        struct CopyRange {
            TCHAR const* name;
            int32 source;
            int32 destination;
            int32 count;
            bool single_element{};
        };
        constexpr CopyRange cases[]{
            {TEXT("source before destination"), 0, 2, 5},
            {TEXT("destination before source"), 2, 0, 5},
            {TEXT("identical ranges"), 1, 1, 5},
            {TEXT("disjoint forward"), 0, 5, 3},
            {TEXT("disjoint backward"), 5, 0, 3},
            {TEXT("one element"), 3, 6, 1, true},
            {TEXT("identical element"), 3, 3, 1, true},
            {TEXT("empty range"), 2, 4, 0},
        };
        constexpr int32 row_count{8};
        for (auto const& range : cases) {
            for (bool const sliced : {false, true}) {
                Owner owner;
                owner.set_num(row_count);
                auto const view{owner.get_view()};
                int32 column_index{};
                auto const initialize_column{[&](auto column) {
                    using Element = std::remove_cvref_t<decltype(column[0])>;
                    for (int32 row{}; row < row_count; ++row) {
                        column[row] = static_cast<Element>(column_index * 16 + row);
                    }
                    ++column_index;
                }};
                view.apply_arrays([&](auto... columns) { (initialize_column(columns), ...); });
                auto const capacity{owner.capacity()};
                auto const source_first{sliced ? range.source : 0};
                auto const source{owner.get_const_view(source_first, row_count - source_first)};
                if (range.single_element) {
                    owner.copy_element(range.destination, source, range.source - source_first);
                } else {
                    owner.copy_elements(
                        range.destination, source, range.source - source_first, range.count);
                }

                auto correct{owner.num() == row_count && owner.capacity() == capacity};
                column_index = 0;
                auto const check_column{[&](auto const column) {
                    using Element = std::remove_cvref_t<decltype(column[0])>;
                    for (int32 row{}; row < row_count; ++row) {
                        auto const copied{row >= range.destination &&
                                          row < range.destination + range.count};
                        auto const original{copied ? range.source + row - range.destination : row};
                        correct &=
                            column[row] == static_cast<Element>(column_index * 16 + original);
                    }
                    ++column_index;
                }};
                view.apply_arrays([&](auto... columns) { (check_column(columns), ...); });
                TestRunner->TestTrue(
                    *FString::Printf(TEXT("%s (sliced source: %d): all columns preserve rows"),
                                     range.name,
                                     static_cast<int32>(sliced)),
                    correct && column_index == 4);
            }
        }
    }

    TEST_METHOD(LogicalCompactApi)
    {
        using namespace ml::soa_test_fixture;
        ApiOwner owner;
        owner.set_num(2);
        auto view{owner.get_view()};
        view.assign_first(7.0f);
        view.values()[1] = 9.0f;
        auto positions{view.view_positions()};
        positions.xs()[0] = 2.0f;
        positions.ys()[0] = 3.0f;
        positions.shift_x(4.0f);
        view.masks()[0].set(ApiField::Values);
        owner.reserve(owner.capacity() + 1);

        TestRunner->TestTrue(TEXT("Exported inline and out-of-line owner API"),
                             owner.first_value() == 7.0f && owner.total() == 16.0f);
        TestRunner->TestTrue(TEXT("Mutable and const compact view functions"),
                             view.first_x() == 6.0f && owner.get_const_view().first_x() == 6.0f);
        TestRunner->TestTrue(TEXT("Nested logical API and equivalent row follow growth"),
                             positions.x_at(0) == 6.0f && positions[0].y == 3.0f);
        TestRunner->TestTrue(TEXT("Single-only field mask"), view.masks()[0].has(ApiField::Values));
        owner.append_from(view.left(1));
        owner.copy_elements(1, owner.get_const_view(), 0, 2);
        TestRunner->TestTrue(TEXT("Compact overlapping copy preserves source rows"),
                             owner.get_const_view().values()[2] == 9.0f);
        EquivalentOwner equivalent;
        equivalent.set_num(1);
        equivalent.get_view().xs()[0] = 5.0f;
        TestRunner->TestTrue(TEXT("Equivalent owner API"), equivalent[0].x == 5.0f);
    }

    TEST_METHOD(CompactHandleFollowsGrowth)
    {
        Owner owner;
        owner.set_num(3);
        owner.get_view().bytes()[1] = 17;
        owner.get_view().view_nested().xs()[1] = 2.5f;

        auto const compact{owner.get_view().slice(1, 2)};
        auto const old_bytes{compact.bytes()};
        auto const old_vector{compact.view_nested()};
        owner.reserve(owner.capacity() + 1);

        TestRunner->TestTrue(TEXT("Compact bytes resolve after growth"), compact.bytes()[0] == 17);
        TestRunner->TestTrue(TEXT("Compact nested vectors resolve after growth"),
                             compact.view_nested().xs()[0] == 2.5f);
        TestRunner->TestTrue(TEXT("Materialized byte view retains its old pointer"),
                             old_bytes.GetData() != compact.bytes().GetData());
        TestRunner->TestTrue(TEXT("Materialized vector view retains its old pointer"),
                             old_vector.xs().GetData() != compact.view_nested().xs().GetData());
    }

    TEST_METHOD(CompactSelfAppendSurvivesGrowth)
    {
        Owner owner;
        owner.set_num(2);
        owner.get_view().bytes()[1] = 19;
        owner.get_view().view_nested().xs()[1] = 3.5f;
        owner.set_num(owner.capacity());

        auto const slice{owner.get_view().slice(1, 1)};
        auto const first{owner.num()};
        auto const before_slice_data{owner.get_view().bytes().GetData()};
        owner.append_from(slice);
        TestRunner->TestTrue(TEXT("Sliced append relocates storage"),
                             owner.get_view().bytes().GetData() != before_slice_data);
        TestRunner->TestTrue(TEXT("Sliced compact self append survives growth"),
                             owner.get_view().bytes()[first] == 19 &&
                                 owner.get_view().view_nested().xs()[first] == 3.5f);

        auto const whole{owner.get_view()};
        auto const old_count{owner.num()};
        auto const before_whole_data{owner.get_view().bytes().GetData()};
        owner.append_from(whole);
        TestRunner->TestTrue(TEXT("Whole append relocates storage"),
                             owner.get_view().bytes().GetData() != before_whole_data);
        TestRunner->TestTrue(TEXT("Whole compact self append survives growth"),
                             owner.get_view().bytes()[old_count + 1] == 19 &&
                                 owner.get_view().view_nested().xs()[old_count + 1] == 3.5f);
    }

    TEST_METHOD(MoveAssignmentReplacesDestinationStorage)
    {
        Owner destination;
        destination.set_num(1);
        auto const old_view{destination.get_view()};
        auto const old_data{destination.get_view().bytes().GetData()};
        auto* const destination_address{&destination};
        TestRunner->TestTrue(TEXT("Retained view initially observes destination storage"),
                             old_view.bytes().GetData() == old_data);

        Owner source;
        source.set_num(1);
        auto const source_data{source.get_view().bytes().GetData()};
        destination = std::move(source);

        TestRunner->TestTrue(TEXT("Destination object remains in place"),
                             &destination == destination_address);
        TestRunner->TestTrue(TEXT("Destination now owns the moved-in allocation"),
                             destination.get_view().bytes().GetData() == source_data);
        TestRunner->TestTrue(TEXT("Previous destination allocation was replaced"),
                             destination.get_view().bytes().GetData() != old_data);
    }
};
