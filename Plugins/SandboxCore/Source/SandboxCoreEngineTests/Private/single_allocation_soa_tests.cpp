#include <SandboxCore/single_allocation/runtime.h>
#include "generated/soa_fixture.h"

#include <CQTest.h>

#include <cstdint>
#include <utility>

TEST_CLASS(SingleAllocationSoa, "SandboxCoreEngine.UnitTests")
{
    using Owner = ml::soa_test_fixture::SingleRows;

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
