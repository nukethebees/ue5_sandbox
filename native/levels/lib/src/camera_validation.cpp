#include "validation.h"

#include <cmath>
#include <format>

namespace ioj::levels::detail {
void validate_camera(LevelCameraDefinition const& camera,
                     EntityValidationState const& entities,
                     Diagnostics& result) {
    if (camera.target_entity_ids.empty()) {
        add_error(result,
                  "level.camera.look-at",
                  DiagnosticCode::MissingCameraTarget,
                  "Initial camera has no target entities");
    }
    IdSet targets;
    auto const target_count{camera.target_entity_ids.size()};
    for (std::size_t index{}; index < target_count; ++index) {
        auto const& id{camera.target_entity_ids[index]};
        if (!targets.insert(id).second) {
            add_error(result,
                      "level.camera.look-at",
                      DiagnosticCode::DuplicateCameraTarget,
                      "Initial camera target " + std::to_string(index) + " duplicates entity '" +
                          id.value + "'");
            continue;
        }
        if (id.empty() || !entities.ids.contains(id)) {
            add_error(result,
                      "level.camera.look-at",
                      DiagnosticCode::CameraTargetNotFound,
                      "Initial camera target '" + id.value + "' is not declared");
        }
    }
    if (!std::isfinite(camera.distance) || camera.distance <= 0.0) {
        add_error(result,
                  "level.camera.distance",
                  DiagnosticCode::InvalidCameraDistance,
                  "Initial camera distance must be finite and greater than zero");
    }
    auto const& direction{camera.offset_direction};
    auto const finite{std::isfinite(direction.x) && std::isfinite(direction.y) &&
                      std::isfinite(direction.z)};
    auto const nearly_zero{std::abs(direction.x) <= 1.0e-4 && std::abs(direction.y) <= 1.0e-4 &&
                           std::abs(direction.z) <= 1.0e-4};
    if (!finite || nearly_zero) {
        add_error(result,
                  "level.camera.offset-direction",
                  DiagnosticCode::InvalidCameraOffsetDirection,
                  "Initial camera offset direction must be finite and non-zero");
    }
}
}
