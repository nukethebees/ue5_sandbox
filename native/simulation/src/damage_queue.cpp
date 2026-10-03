#include <ioj/sim/damage_queue.h>

#include <ioj/sim/profiling.h>

#include <sandbox/core/frame_array.h>

namespace ioj::sim {
void DamageQueue::prepare(EntityLookupTables const& indexes,
                          ml::FrameMemoryResource* const scratch_resource) {
    SANDBOX_PROFILE_SCOPE("DamageQueue::prepare");
    spans_ = {};
    auto const count{events_.num()};
    if (count == 0) {
        return;
    }

    std::uint32_t live_count{};
    for (std::uint32_t i{}; i < count; ++i) {
        auto const id{events_.damaged_entities[i]};
        assert(id.is_valid());
        if (live_count != i) {
            events_.copy_element(live_count, events_, i);
        }
        ++live_count;
        ++spans_[id.entity_type()].count;
    }
    events_.set_num(live_count);

    EntityTypeSizes write_offsets{};
    std::uint32_t offset{};
    std::uint32_t groups{};
    auto const entity_type_count{EntityTypeSizes::size()};
    for (std::size_t i{}; i < entity_type_count; ++i) {
        auto const type{static_cast<EntityType>(i)};
        auto& span{spans_[type]};
        span.offset = offset;
        write_offsets[type] = static_cast<std::uint32_t>(offset);
        offset += span.count;
        groups += span.count > 0 ? 1 : 0;
    }
    if (groups > 1) {
        ml::FrameArray<std::int32_t> order{scratch_resource};
        order.set_num(live_count);
        for (std::uint32_t i{}; i < live_count; ++i) {
            auto const type{events_.damaged_entities[i].entity_type()};
            auto const destination{static_cast<std::uint32_t>(write_offsets[type]++)};
            order[destination] = static_cast<std::int32_t>(i);
        }
        events_.apply_permutation(order);
    }

    // Filter retired recipients with one bound lookup table per type run.
    std::uint32_t destination{};
    for (auto const type : ml::EnumTraits<EntityType>::values) {
        auto const source{spans_[type]};
        auto const end{source.end()};
        // NOLINTNEXTLINE(ioj-loop-view-accessor-call) -- bind a different table for each type run.
        auto const handles{indexes.for_type(type).entries()};
        auto const handle_count{handles.size()};
        auto const first{destination};
        for (auto row{source.offset}; row < end; ++row) {
            auto const id{events_.damaged_entities[row]};
            if (id.index() >= handle_count || !handles[id.index()].is_valid()) {
                continue;
            }
            if (destination != row) {
                events_.copy_element(destination, events_, row);
            }
            ++destination;
        }
        spans_[type] = {first, destination - first};
    }
    events_.set_num(destination);
}
}
