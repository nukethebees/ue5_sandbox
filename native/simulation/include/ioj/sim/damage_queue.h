#pragma once

#include <ioj/sim/agent_indexes.h>
#include <ioj/sim/direct_damage_events.h>
#include <ioj/sim/index_span.h>
#include <sandbox/core/enum_array.h>

#include <memory_resource>

namespace ioj::sim {
class DamageQueue {
  public:
    void append(DirectDamageEventsConstView events) { events_.append_from(events); }
    void reset() {
        events_.reset();
        spans_ = {};
    }
    void prepare(AgentIndexes const& indexes, std::pmr::memory_resource& scratch);
    auto events_for(EntityType type) const -> DirectDamageEventsConstView {
        auto const span{spans_[type]};
        return events_.get_const_view(span.offset, span.count);
    }
    auto all_events() const -> DirectDamageEvents const& { return events_; }
  private:
    DirectDamageEvents events_;
    ml::EnumArray<EntityType, IndexSpan, static_cast<std::size_t>(EntityType::COUNT)> spans_{};
};
}
