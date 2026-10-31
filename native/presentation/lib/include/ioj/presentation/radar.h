#pragma once

#include <ioj/presentation/agent_display_batch.h>
#include <ioj/settings/radar.h>
#include <ioj/transform3d.h>
#include <ioj/ui/entity_overlay.h>
#include <ioj/ui/radar.h>
#include <ioj/vector.h>

#include <vector>

namespace ioj::presentation {
auto sanitize_radar_settings(settings::Radar settings) -> settings::Radar;

struct RadarContactColours {
    ui::Color4f friendly{0.12f, 0.72f, 1.0f, 1.0f};
    ui::Color4f hostile{1.0f, 0.08f, 0.035f, 1.0f};
    ui::Color4f neutral{0.78f, 0.84f, 0.86f, 1.0f};
};

struct RadarFrame {
    std::vector<ui::radar::Instance> instances;
    float combat_display_radius{0.45f};
    float tactical_display_radius{0.8f};
};

struct RadarCollectionResult {
    std::int32_t candidate_count{};
    std::int32_t visible_count{};
};

// Objective roles follow the concatenated batch rows. The player marker is emitted last.
auto collect_radar_instances(std::span<AgentDisplayBatch const> batches,
                             std::span<ui::entity_overlay::ObjectiveRole const> objective_roles,
                             RadarContactColours const& contact_colours,
                             Transform3d const& player_transform,
                             sim::EntityId player_id,
                             Team player_team,
                             settings::Radar const& settings,
                             RadarFrame& output_frame) -> RadarCollectionResult;

namespace radar {
auto curve_parameter(settings::Radar const& settings) -> float;
auto map_distance(float normalized_distance, float curve) -> float;
auto to_radar_display_radius(float world_distance, settings::Radar const& settings) -> float;
auto to_radar_position(Vector3f local_delta, settings::Radar const& settings, float curve)
    -> Vector3f;
auto to_radar_position(Vector3f local_delta, settings::Radar const& settings) -> Vector3f;
}
}
