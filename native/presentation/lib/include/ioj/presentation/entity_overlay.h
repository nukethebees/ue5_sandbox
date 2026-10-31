#pragma once

#include <ioj/presentation/agent_display_batch.h>
#include <ioj/presentation/entity_overlay_collection_result.h>
#include <ioj/settings/entity_overlay.h>
#include <ioj/sim/entities/entity_type_radii.h>
#include <ioj/ui/entity_overlay.h>

#include <vector>

namespace ioj::presentation {
struct EntityOverlayHealthMaximums;
struct EntityOverlayTeamColours;

auto sanitize_entity_overlay_settings(settings::EntityOverlay settings) -> settings::EntityOverlay;

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
