#pragma once

#include "sandbox/core/ui/types.h"

#include <cstdint>
#include <optional>

namespace ml::ui::entity_overlay {
enum class ObjectiveRole : std::uint32_t {
    None,
    Defend,
    Destroy,
};

struct Instance {
    Vector3f world_position{};
    float health{};
    float world_radius{};
    std::uint32_t display_data{};
};

struct Addition {
    Instance instance{};
    std::int32_t swap_index{-1};
};

[[nodiscard]] auto source_sizes_match(std::size_t position_count,
                                      std::size_t health_count,
                                      std::size_t radius_count) noexcept -> bool;
[[nodiscard]] auto pack_display_data(ObjectiveRole role) noexcept -> std::uint32_t;
[[nodiscard]] auto pack_display_data(ObjectiveRole role, Color4f fill_color) noexcept
    -> std::uint32_t;

class Collector {
  public:
    void begin(Vector3f origin, float maximum_range) noexcept;
    [[nodiscard]] auto try_add(Vector3f position,
                               float normalized_health,
                               float world_radius,
                               std::uint32_t display_data,
                               bool bypass_range = false) noexcept -> std::optional<Addition>;

    [[nodiscard]] auto invalid_health_count() const noexcept -> std::int32_t {
        return invalid_health_count_;
    }
  private:
    Vector3f origin_{};
    float maximum_range_squared_{};
    std::int32_t first_objective_index_{-1};
    std::int32_t accepted_count_{};
    std::int32_t invalid_health_count_{};
};
}
