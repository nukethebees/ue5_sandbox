#pragma once

#include <ioj/enum_array.h>
#include <ioj/frame_memory_resource.h>
#include <ioj/sim/direct_damage_events.h>
#include <ioj/sim/entities/entity_lookup_table.h>
#include <ioj/sim/index_span.h>

namespace ioj::sim {
class DamageQueue {
  public:
    void append(DirectDamageEvents::ConstView events) { events_.append_from(events); }
    void reset() {
        events_.reset();
        spans_ = {};
    }
    void prepare(EntityLookupTables const& indexes, FrameMemoryResource* const scratch_resource);
    auto events_for(EntityType type) const -> DirectDamageEvents::ConstView {
        auto const span{spans_[type]};
        return events_.get_const_view(span.offset, span.count);
    }
    auto all_events() const -> DirectDamageEvents const& { return events_; }
  private:
    DirectDamageEvents events_;
    EnumArray<EntityType, IndexSpan> spans_{};
};
}
