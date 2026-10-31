#pragma once

#include <ioj/enum_array.h>
#include <ioj/presentation/agent_display_batch.h>
#include <ioj/settings/entity_overlay.h>
#include <ioj/sim/entities/entity_type_radii.h>
#include <ioj/ui/entity_overlay.h>

#include <vector>

namespace ioj::presentation {
auto sanitize_entity_overlay_settings(settings::EntityOverlay settings) -> settings::EntityOverlay;

struct EntityOverlayHealthMaximums {
    std::int32_t capital_ship{1};
    std::int32_t fighter{1};
    std::int32_t turret{1};
};

struct EntityOverlayTeamColours {
    EnumArray<Team, ui::Color4f> capital_ship{ui::Color4f{1.f, 0.f, 1.f, 1.f}};
    EnumArray<Team, ui::Color4f> fighter{ui::Color4f{1.f, 0.f, 1.f, 1.f}};
    EnumArray<Team, ui::Color4f> turret{ui::Color4f{1.f, 0.f, 1.f, 1.f}};
};

struct EntityOverlayCollectionResult {
    std::int32_t candidate_count{};
    std::int32_t invalid_health_count{};
};

// Objective roles follow the concatenated batch rows. Objective contacts bypass the range limit.
auto collect_entity_overlay_instances(
    std::span<AgentDisplayBatch const> batches,
    sim::EntityTypeRadii const& entity_type_radii,
    std::span<ui::entity_overlay::ObjectiveRole const> objective_roles,
    EntityOverlayTeamColours const& team_colours,
    EntityOverlayHealthMaximums const& maximum_health,
    Vector3f origin,
    float maximum_range,
    std::vector<ui::entity_overlay::Instance>& output_instances) -> EntityOverlayCollectionResult;
}
