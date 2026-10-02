#pragma once

#include "pmr_test_support.h"

#include <type_traits>
#include <utility>

namespace pmr_test_support {

template <typename Owner, typename FirstValue>
void verify_pmr_owner(FirstValue first_value) {
    static_assert(sizeof(Owner) == 24);
    static_assert(sizeof(typename Owner::View) == 16);
    static_assert(std::is_nothrow_move_constructible_v<Owner>);
    static_assert(!std::is_nothrow_move_assignable_v<Owner>);
    static_assert(!std::is_copy_constructible_v<Owner>);
    static_assert(!std::is_convertible_v<std::pmr::memory_resource*, Owner>);

    AllocationState first_state;
    AllocationState second_state;
    CountingResource first{first_state};
    CountingResource second{second_state};
    {
        DefaultResourceScope use_first{&first};
        Owner empty;
        EXPECT_EQ(empty.get_memory_resource(), &first);
        EXPECT_EQ(first_state.allocations, 0);
        {
            DefaultResourceScope use_second{&second};
            Owner current_default;
            EXPECT_EQ(current_default.get_memory_resource(), &second);
            empty.set_num(1);
            EXPECT_EQ(first_state.allocations, 1);
            EXPECT_EQ(second_state.allocations, 0);
            EXPECT_EQ(first_state.last_bytes, empty.allocated_bytes());
            EXPECT_EQ(first_state.last_alignment, Owner::allocation_alignment);
        }
    }
    EXPECT_TRUE(first_state.live.empty());
    EXPECT_EQ(first_state.allocations, first_state.frees);
    EXPECT_EQ(second_state.frees, 0);

    // Different resource objects may permit transfer when they share an allocation domain.
    CountingResource equivalent{first_state};
    first.set_equal_resource(equivalent);
    {
        Owner source{&first};
        source.set_num(2);
        first_value(source) = 73;
        auto* const original{&first_value(source)};
        auto const allocations{first_state.allocations};
        Owner moved{std::move(source)};
        EXPECT_EQ(moved.get_memory_resource(), &first);
        EXPECT_EQ(&first_value(moved), original);
        EXPECT_EQ(source.num(), 0);
        EXPECT_EQ(source.capacity(), 0);
        EXPECT_EQ(source.get_memory_resource(), &first);
        EXPECT_EQ(first_state.allocations, allocations);

        Owner destination{&equivalent};
        destination.set_num(1);
        auto const before_move{first_state.allocations};
        destination = std::move(moved);
        EXPECT_EQ(destination.get_memory_resource(), &equivalent);
        EXPECT_EQ(&first_value(destination), original);
        EXPECT_EQ(first_value(destination), 73);
        EXPECT_EQ(first_state.allocations, before_move);
        EXPECT_EQ(moved.capacity(), 0);
        EXPECT_EQ(moved.get_memory_resource(), &first);

        auto& alias{destination};
        destination = std::move(alias);
        EXPECT_EQ(&first_value(destination), original);
        EXPECT_EQ(destination.num(), 2);
        source.set_num(1);
        EXPECT_EQ(source.get_memory_resource(), &first);
    }
    EXPECT_TRUE(first_state.live.empty());
    EXPECT_EQ(first_state.allocations, first_state.frees);

    for (auto const reserve : {0u, 64u, 256u}) {
        Owner source{&first};
        source.set_num(128);
        first_value(source) = 91;
        auto* const source_data{&first_value(source)};
        Owner destination{&second};
        destination.reserve(reserve);
        if (reserve > 0) {
            destination.set_num(1);
            first_value(destination) = 42;
        }
        auto const old_capacity{destination.capacity()};
        auto const old_count{destination.num()};
        auto const old_allocations{second_state.allocations};
        if (old_capacity < source.num()) {
            second_state.reject_allocation = true;
            EXPECT_THROW(destination = std::move(source), std::bad_alloc);
            second_state.reject_allocation = false;
            EXPECT_EQ(destination.capacity(), old_capacity);
            EXPECT_EQ(destination.num(), old_count);
            if (old_count > 0) {
                EXPECT_EQ(first_value(destination), 42);
            }
            EXPECT_EQ(source.num(), 128);
            EXPECT_EQ(first_value(source), 91);
            EXPECT_EQ(&first_value(source), source_data);
        }

        destination = std::move(source);
        EXPECT_EQ(destination.get_memory_resource(), &second);
        EXPECT_EQ(source.get_memory_resource(), &first);
        EXPECT_EQ(destination.num(), 128);
        EXPECT_EQ(first_value(destination), 91);
        EXPECT_NE(&first_value(destination), source_data);
        EXPECT_EQ(second_state.allocations, old_allocations + (old_capacity < 128 ? 1 : 0));
        EXPECT_EQ(source.num(), 0);
        EXPECT_EQ(source.capacity(), 128);
        source.set_num(1);
        EXPECT_EQ(&first_value(source), source_data);
        first_value(source) = 12;
        EXPECT_EQ(first_value(destination), 91);

        Owner empty{&first};
        auto const destination_capacity{destination.capacity()};
        destination = std::move(empty);
        EXPECT_EQ(destination.num(), 0);
        EXPECT_EQ(destination.capacity(), destination_capacity);
        EXPECT_EQ(destination.get_memory_resource(), &second);
    }
    EXPECT_TRUE(first_state.live.empty());
    EXPECT_TRUE(second_state.live.empty());
    EXPECT_EQ(first_state.allocations, first_state.frees);
    EXPECT_EQ(second_state.allocations, second_state.frees);
}

}
