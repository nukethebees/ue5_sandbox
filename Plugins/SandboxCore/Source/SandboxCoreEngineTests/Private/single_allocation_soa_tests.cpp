#include "generated/soa_fixture.h"

#include <SandboxCore/single_allocation/runtime.h>
#include <sbx/memory.h>
#include <sbx/memory_resource.h>

#include <CQTest.h>

#include <array>
#include <utility>

TEST_CLASS(SingleAllocationSoa, "SandboxCoreEngine.UnitTests")
{
    using Owner = ml::soa_test_fixture::SingleRows;

    TEST_METHOD(PmrResourcesControlConstructionAndMoveAssignment)
    {
        Owner defaults;
        TestRunner->TestTrue(TEXT("Default construction captures the PMR default"),
                             defaults.get_memory_resource() == std::pmr::get_default_resource());
        std::array<std::byte, 8192> first_buffer{};
        std::array<std::byte, 8192> second_buffer{};
        std::pmr::monotonic_buffer_resource first{first_buffer.data(), first_buffer.size()};
        std::pmr::monotonic_buffer_resource second{second_buffer.data(), second_buffer.size()};
        Owner source{&first};
        source.set_num(2);
        source.get_view().bytes()[0] = 31;
        source.get_view().view_nested().xs()[0] = 2.5f;
        Owner destination{&second};
        destination.set_num(1);
        auto const* destination_data{destination.get_view().bytes().GetData()};
        destination = std::move(source);
        TestRunner->TestTrue(TEXT("Move assignment retains the destination resource and capacity"),
                             destination.get_memory_resource() == &second &&
                                 destination.get_view().bytes().GetData() == destination_data);
        TestRunner->TestTrue(TEXT("Unequal-resource move copies nested columns"),
                             destination.num() == 2 && destination.get_view().bytes()[0] == 31 &&
                                 destination.get_view().view_nested().xs()[0] == 2.5f &&
                                 source.num() == 0 && source.get_memory_resource() == &first);
    }

    TEST_METHOD(LogicalCompactApi)
    {
        using namespace ml::soa_test_fixture;
        ApiOwner owner{sbx::memory::mimalloc_resource()};
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
        EquivalentOwner equivalent{sbx::memory::mimalloc_resource()};
        equivalent.set_num(1);
        equivalent.get_view().xs()[0] = 5.0f;
        TestRunner->TestTrue(TEXT("Equivalent owner API"), equivalent[0].x == 5.0f);
    }

    TEST_METHOD(ReacquiredViewsPreserveValuesAfterGrowth)
    {
        Owner owner{sbx::memory::mimalloc_resource()};
        owner.set_num(3);
        TestRunner->TestTrue(TEXT("Explicit mimalloc resource owns the allocation"),
                             sbx::memory::owns(owner.get_view().bytes().GetData()));
        owner.get_view().bytes()[1] = 17;
        owner.get_view().view_nested().xs()[1] = 2.5f;

        owner.reserve(owner.capacity() + 1);

        auto const compact{owner.get_view().slice(1, 2)};
        TestRunner->TestTrue(TEXT("Reacquired bytes preserve values"), compact.bytes()[0] == 17);
        TestRunner->TestTrue(TEXT("Reacquired nested vectors preserve values"),
                             compact.view_nested().xs()[0] == 2.5f);
    }
};
