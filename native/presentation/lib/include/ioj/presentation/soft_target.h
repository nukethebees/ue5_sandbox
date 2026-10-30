#pragma once

#include <ioj/constrained_numbers.h>
#include <ioj/presentation/agent_display_batch.h>
#include <ioj/settings/soft_target.h>
#include <ioj/sim/entities/entity_type_radii.h>
#include <ioj/ui/entity_overlay.h>

#include <array>
#include <optional>

namespace ioj::presentation {
struct OverlayView {
    auto is_valid() const noexcept -> bool { return size.x() > 0 && size.y() > 0; }

    Vector3f camera_origin{};
    // Row-vector projection of positions relative to camera_origin.
    std::array<std::array<float, 4>, 4> view_projection{};
    Vector2f minimum{};
    Vector2f size{};
};

struct SoftTargetContext {
    OverlayView view;
    Vector3f aim_origin{};
    Vector3f aim_direction{1.f, 0.f, 0.f};
    Vector3f camera_right{0.f, 1.f, 0.f};
    Vector3f camera_up{0.f, 0.f, 1.f};
    Team player_team{Team::White};
    float effective_weapon_range{};
    float maximum_overlay_range{};
};

inline constexpr auto soft_target_range_policy{
    ConstrainedValuePolicy{[](float const value) { return value >= 0.f && value <= 1.f; },
                           "Soft-target range alpha must be in [0, 1]"}};

using SoftTargetRange = ConstrainedValue<float, soft_target_range_policy>;

struct SoftTargetResult {
    sim::EntityId id{};
    SoftTargetRange range_alpha{};
    float indicator_radius_pixels{};
    float world_units_per_pixel{};
    bool previous_target_can_fade{};
};

struct SoftTargetVisual {
    sim::EntityId entity{};
    SoftTargetRange range_alpha{};
    float radius_pixels{};
    float world_units_per_pixel{};
};

struct SoftTargetMarker {
    SoftTargetVisual visual{};
    std::optional<Vector3f> location{};
    float pulse{};
    float visibility{};
};

struct SoftTargetTiming {
    NonNegativeFinite<float> delta_seconds{};
    NonNegativeFinite<float> pulse_duration{};
    NonNegativeFinite<float> fade_duration{};
};

class SoftTargetState {
  public:
    auto advance(std::span<AgentDisplayBatch const> batches,
                 SoftTargetResult const& selected,
                 SoftTargetTiming timing) -> std::array<SoftTargetMarker, 2>;

    auto current_target() const noexcept -> sim::EntityId { return active_.entity; }
  private:
    SoftTargetVisual active_{};
    SoftTargetVisual fading_{};
    float pulse_remaining_{};
    float fade_remaining_{};
};

auto select_soft_target(std::span<AgentDisplayBatch const> batches,
                        sim::EntityTypeRadii const& entity_type_radii,
                        std::span<ui::entity_overlay::ObjectiveRole const> objective_roles,
                        SoftTargetContext const& context,
                        settings::SoftTarget const& settings,
                        sim::EntityId current_target) -> SoftTargetResult;
}
