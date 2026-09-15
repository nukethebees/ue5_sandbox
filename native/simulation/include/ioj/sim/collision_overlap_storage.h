#pragma once

#include "ioj/sim/collision_events.h"
#include "ioj/sim/entity_handle.h"
#include "ioj/sim/entity_overlaps.h"

#include <cstdint>
#include <vector>

namespace ioj::sim::collision {
class CollisionOverlapStorage {
  public:
    void reset() noexcept;
    void clear() noexcept;
    void add_entity_overlap(RegistryEntityHandle first, RegistryEntityHandle second);
    void add_static_overlap(RegistryEntityHandle entity, std::int32_t static_geometry_index);
    void finalize();

    [[nodiscard]] auto get_view() const noexcept -> DetectedOverlapsView;
    [[nodiscard]] auto entity_entity_overlaps() const noexcept -> EntityEntityOverlapsConstView;
    [[nodiscard]] auto entity_static_overlaps() const noexcept -> EntityStaticOverlapsConstView;
  private:
    EntityEntityOverlaps entity_entity_overlaps_;
    EntityStaticOverlaps entity_static_overlaps_;
    std::vector<std::int32_t> sort_indices_scratch_;
};
} // namespace ioj::sim::collision
