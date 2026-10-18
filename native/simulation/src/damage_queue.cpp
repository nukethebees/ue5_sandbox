#include <ioj/sim/damage_queue.h>

#include <ioj/frame_array.h>
#include <ioj/sim/profiling.h>
#include <ioj/soa_permutation.h>

namespace ioj::sim {
void DamageQueue::prepare(EntityLookupTables const& indexes,
                          FrameMemoryResource* const scratch_resource) {
    SANDBOX_PROFILE_SCOPE("DamageQueue::prepare");
    spans_ = {};
    auto const count{events_.num()};
    if (count == 0) {
        return;
    }

    auto const damaged_entities{events_.get_const_view().damaged_entities()};
    for (std::uint32_t i{}; i < count; ++i) {
        auto const id{damaged_entities[i]};
        assert(id.is_valid());
        ++spans_[id.entity_type()].count;
    }

    EntityTypeSizes write_offsets{};
    std::uint32_t offset{};
    std::uint32_t groups{};
    auto const entity_type_count{EntityTypeSizes::size()};
    // Assign contiguous type runs and count nonempty groups before reordering damage events.
    for (std::size_t i{}; i < entity_type_count; ++i) {
        auto const type{static_cast<EntityType>(i)};
        auto& span{spans_[type]};
        span.offset = offset;
        write_offsets[type] = static_cast<std::uint32_t>(offset);
        offset += span.count;
        groups += span.count > 0 ? 1 : 0;
    }
    if (groups > 1) {
        FrameArray<std::int32_t> order{scratch_resource};
        order.set_num(count);
        for (std::uint32_t i{}; i < count; ++i) {
            auto const type{damaged_entities[i].entity_type()};
            auto const destination{static_cast<std::uint32_t>(write_offsets[type]++)};
            order[destination] = static_cast<std::int32_t>(i);
        }
        auto const events{events_.get_view()};
        auto const recipients{events.damaged_entities()};
        auto const amounts{events.damage_amounts()};
        auto const instigators{events.instigators()};
        auto const permutation{order.view()};
        apply_permutation(recipients, permutation);
        apply_permutation(amounts, permutation);
        apply_permutation(instigators, permutation);
    }

    // Filter retired recipients with one bound lookup table per type run.
    std::uint32_t destination{};
    auto const sorted_events{events_.get_const_view()};
    auto const sorted_entities{sorted_events.damaged_entities()};
    for (auto const type : EnumTraits<EntityType>::values) {
        auto const source{spans_[type]};
        auto const end{source.end()};
        // NOLINTNEXTLINE(ioj-loop-view-accessor-call) -- bind a different table for each type run.
        auto const handles{indexes.for_type(type).entries()};
        auto const handle_count{handles.size()};
        auto const first{destination};
        for (auto row{source.offset}; row < end; ++row) {
            auto const id{sorted_entities[row]};
            if (id.index() >= handle_count || !handles[id.index()].is_valid()) {
                continue;
            }
            if (destination != row) {
                events_.copy_element(destination, sorted_events, row);
            }
            ++destination;
        }
        spans_[type] = {first, destination - first};
    }
    events_.set_num(destination);
}
}
