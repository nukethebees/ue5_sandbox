#pragma once

#include "sandbox/simulation/collision_events.h"
#include "sandbox/simulation/entity_handle.h"
#include "sandbox/simulation/entity_overlaps.h"

#include <cstdint>
#include <vector>

namespace ml::simulation::collision {
class CollisionOverlapStorage {
  public:
    void reset() noexcept;
    void clear() noexcept;
    void add_entity_overlap(FRegistryEntityHandle first, FRegistryEntityHandle second);
    void add_static_overlap(FRegistryEntityHandle entity, std::int32_t static_geometry_index);
    void finalize();

    [[nodiscard]] auto get_view() const noexcept -> DetectedOverlapsView;
    [[nodiscard]] auto entity_entity_overlaps() const noexcept
        -> ioj::FEntityEntityOverlapsConstView;
    [[nodiscard]] auto entity_static_overlaps() const noexcept
        -> ioj::FEntityStaticOverlapsConstView;
  private:
    ioj::FEntityEntityOverlaps entity_entity_overlaps_;
    ioj::FEntityStaticOverlaps entity_static_overlaps_;
    std::vector<std::int32_t> sort_indices_scratch_;
};
} // namespace ml::simulation::collision
