#include <ioj/sim/damage_queue.h>
#include <ioj/sim/profiling.h>
#include <sandbox/core/frame_array.h>

namespace ioj::sim {
void DamageQueue::prepare(AgentIndexes const& indexes, ml::FrameScratch& scratch) {
    SANDBOX_PROFILE_SCOPE("DamageQueue::prepare");
    spans_ = {};
    auto const count{events_.num()};
    if (count == 0) {
        return;
    }

    std::int32_t live_count{};
    for (std::int32_t i{}; i < count; ++i) {
        auto const id{events_.damaged_entities[i]};
        assert(id.is_valid());
        if (indexes.find(id) < 0) {
            continue;
        }
        if (live_count != i) {
            events_.copy_element(live_count, events_, i);
        }
        ++live_count;
        ++spans_[id.entity_type()].count;
    }
    events_.set_num(live_count);

    EntityTypeSizes write_offsets{};
    std::int32_t offset{};
    std::int32_t groups{};
    for (std::size_t i{}; i < EntityTypeSizes::size(); ++i) {
        auto const type{static_cast<EntityType>(i)};
        auto& span{spans_[type]};
        span.offset = offset;
        write_offsets[type] = static_cast<std::uint32_t>(offset);
        offset += span.count;
        groups += span.count > 0 ? 1 : 0;
    }
    if (groups <= 1) {
        return;
    }

    ml::FrameArray<std::int32_t> order{&scratch};
    order.set_num(live_count);
    for (std::int32_t i{}; i < live_count; ++i) {
        auto const type{events_.damaged_entities[i].entity_type()};
        auto const destination{static_cast<std::int32_t>(write_offsets[type]++)};
        order[destination] = i;
    }
    events_.apply_permutation(order);
}
}
