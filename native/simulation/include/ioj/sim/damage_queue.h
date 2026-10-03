#pragma once

#include <ioj/sim/direct_damage_events.h>
#include <ioj/sim/entity_lookup_table.h>
#include <ioj/sim/index_span.h>

#include <sandbox/core/enum_array.h>
#include <sandbox/core/frame_memory_resource.h>

namespace ioj::sim {
class DamageQueue {
  public:
    void append(DirectDamageEventsConstView events) { events_.append_from(events); }
    void reset() {
        events_.reset();
        spans_ = {};
    }
    void prepare(EntityLookupTables const& indexes,
                 ml::FrameMemoryResource* const scratch_resource);
    auto events_for(EntityType type) const -> DirectDamageEventsConstView {
        auto const span{spans_[type]};
        return events_.get_const_view(span.offset, span.count);
    }
    auto all_events() const -> DirectDamageEvents const& { return events_; }
  private:
    DirectDamageEvents events_;
    ml::EnumArray<EntityType, IndexSpan> spans_{};
};
}
