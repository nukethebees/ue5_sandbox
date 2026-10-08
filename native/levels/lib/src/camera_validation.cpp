#include "validation.h"

#include <cmath>
#include <format>

namespace ioj::levels::detail {
void validate_camera(LevelCameraDefinition const& camera,
                     EntityValidationState const& entities,
                     Diagnostics& result) {
    if (camera.target_entity_ids.empty()) {
        result.emplace_back(DiagnosticCode::MissingCameraTarget,
                            "level.camera.look-at",
                            "Initial camera has no target entities");
    }
    IdSet targets;
    auto const target_count{camera.target_entity_ids.size()};
    for (std::size_t index{}; index < target_count; ++index) {
        auto const& id{camera.target_entity_ids[index]};
        if (!targets.insert(id).second) {
            result.emplace_back(DiagnosticCode::DuplicateCameraTarget,
                                "level.camera.look-at",
                                "Initial camera target " + std::to_string(index) +
                                    " duplicates entity '" + id.value + "'");
            continue;
        }
        if (id.empty() || !entities.by_id.contains(id)) {
            result.emplace_back(DiagnosticCode::CameraTargetNotFound,
                                "level.camera.look-at",
                                "Initial camera target '" + id.value + "' is not declared");
        }
    }
    if (!std::isfinite(camera.distance) || camera.distance <= 0.0) {
        result.emplace_back(DiagnosticCode::InvalidCameraDistance,
                            "level.camera.distance",
                            "Initial camera distance must be finite and greater than zero");
    }
    auto const& direction{camera.offset_direction};
    auto const finite{ml::is_finite(direction)};
    auto const nearly_zero{std::abs(direction.x) <= 1.0e-4 && std::abs(direction.y) <= 1.0e-4 &&
                           std::abs(direction.z) <= 1.0e-4};
    if (!finite || nearly_zero) {
        result.emplace_back(DiagnosticCode::InvalidCameraOffsetDirection,
                            "level.camera.offset-direction",
                            "Initial camera offset direction must be finite and non-zero");
    }
}
}
