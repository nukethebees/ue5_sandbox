#include "sandbox/core/ui/entity_overlay.h"

#include <algorithm>
#include <cmath>

namespace ml::ui::entity_overlay {
namespace {
constexpr std::uint32_t objective_role_mask{0x3};
constexpr std::uint32_t has_fill_color_mask{1U << 2U};
constexpr std::uint32_t fill_color_red_shift{8};
constexpr std::uint32_t fill_color_green_shift{16};
constexpr std::uint32_t fill_color_blue_shift{24};

auto pack_unorm8(float const value) noexcept -> std::uint32_t {
    return static_cast<std::uint32_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
}
}

auto source_sizes_match(std::size_t const position_count,
                        std::size_t const health_count,
                        std::size_t const radius_count) noexcept -> bool {
    return position_count == health_count && position_count == radius_count;
}

auto pack_display_data(ObjectiveRole const role) noexcept -> std::uint32_t {
    return static_cast<std::uint32_t>(role) & objective_role_mask;
}

auto pack_display_data(ObjectiveRole const role, Color4f const fill_color) noexcept
    -> std::uint32_t {
    return pack_display_data(role) | has_fill_color_mask |
           (pack_unorm8(fill_color.r) << fill_color_red_shift) |
           (pack_unorm8(fill_color.g) << fill_color_green_shift) |
           (pack_unorm8(fill_color.b) << fill_color_blue_shift);
}

void Collector::begin(Vector3f const origin, float const maximum_range) noexcept {
    origin_ = origin;
    maximum_range_squared_ = maximum_range * maximum_range;
    first_objective_index_ = -1;
    accepted_count_ = 0;
    invalid_health_count_ = 0;
}

auto Collector::try_add(Vector3f const position,
                        float normalized_health,
                        float world_radius,
                        std::uint32_t const display_data,
                        bool const bypass_range) noexcept -> std::optional<Addition> {
    auto const dx{position.x - origin_.x};
    auto const dy{position.y - origin_.y};
    auto const dz{position.z - origin_.z};
    auto const distance_squared{dx * dx + dy * dy + dz * dz};
    if (!bypass_range && distance_squared > maximum_range_squared_) {
        return std::nullopt;
    }
    if (!std::isfinite(normalized_health)) {
        normalized_health = 0.0f;
        ++invalid_health_count_;
    }
    if (!std::isfinite(world_radius)) {
        world_radius = 0.0f;
    }

    Addition addition{.instance = {.world_position = position,
                                   .health = std::clamp(normalized_health, 0.0f, 1.0f),
                                   .world_radius = std::max(world_radius, 0.0f),
                                   .display_data = display_data}};
    auto const objective{(display_data & objective_role_mask) != 0};
    if (objective) {
        if (first_objective_index_ < 0) {
            first_objective_index_ = accepted_count_;
        }
    } else if (first_objective_index_ >= 0) {
        addition.swap_index = first_objective_index_;
        ++first_objective_index_;
    }
    ++accepted_count_;
    return addition;
}
}
