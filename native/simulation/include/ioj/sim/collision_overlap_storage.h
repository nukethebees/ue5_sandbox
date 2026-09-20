#pragma once

#include "ioj/sim/collision_events.h"
#include "ioj/sim/entity_overlaps.h"
#include "ioj/sim/entity_unique_id.h"

#include <cstdint>

namespace ml {
class FrameScratch;
}

namespace ioj::sim::collision {
class CollisionOverlapStorage {
  public:
    void reset() noexcept;
    void clear() noexcept;
    void add_entity_overlap(EntityUniqueId first, EntityUniqueId second);
    void add_static_overlap(EntityUniqueId entity, std::int32_t static_geometry_index);
    void finalize(ml::FrameScratch& scratch);

    [[nodiscard]] auto get_view() const noexcept -> DetectedOverlapsView;
    [[nodiscard]] auto entity_entity_overlaps() const noexcept -> EntityEntityOverlapsConstView;
    [[nodiscard]] auto entity_static_overlaps() const noexcept -> EntityStaticOverlapsConstView;
  private:
    EntityEntityOverlaps entity_entity_overlaps_;
    EntityStaticOverlaps entity_static_overlaps_;
};
} // namespace ioj::sim::collision
