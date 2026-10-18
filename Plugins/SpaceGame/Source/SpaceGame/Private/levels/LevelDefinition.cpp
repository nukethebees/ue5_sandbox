#include "SpaceGame/levels/LevelDefinition.h"

#include <SpaceGame/levels/LevelEntityTableOperations.h>
#include <SpaceGame/levels/native_level_definition_conversion.h>

#include <SandboxCoreEngine/strings.h>

namespace ml {
void FLevelBuilder::set_metadata(FLevelMetadata const& metadata) {
    definition_.metadata = metadata;
}

void FLevelBuilder::set_player_entity(FLevelEntityId const id) {
    definition_.player_entity_id = id;
}

void FLevelBuilder::set_camera(FLevelCameraDefinition const& camera) {
    definition_.camera = camera;
}

void FLevelBuilder::set_collision_grid(FLevelCollisionGridDefinition const& collision_grid) {
    definition_.collision_grid = collision_grid;
}

void FLevelBuilder::set_mission(FLevelMissionDefinition const& mission) {
    definition_.mission = mission;
}

void FLevelBuilder::add_unlock_criterion(FLevelUnlockCriterion criterion) {
    definition_.unlock_criteria.Add(MoveTemp(criterion));
}

void FLevelBuilder::add_mission_event(FLevelMissionObjectiveEvent const& event) {
    definition_.mission_events.Add(event);
}

auto FLevelBuilder::add_entity(FEntitySpawnDefinition const& entity) -> FLevelEntityId {
    level_entity_table_detail::append(definition_.entities, entity);
    return entity.id;
}

auto FLevelBuilder::finish() -> FLevelDefinition {
    auto definition{MoveTemp(definition_)};
    definition_ = FLevelDefinition{};
    return definition;
}

auto validate_level(FLevelDefinition const& definition) -> FLevelValidationResult {
    FLevelValidationResult result;
    auto native_result{::ioj::levels::authoring::to_native(definition)};
    auto validation{native_result ? ::ioj::levels::validate_level(*native_result)
                                  : std::expected<void, ::ioj::levels::Diagnostics>{
                                        std::unexpected{std::move(native_result.error())}}};
    if (validation) {
        return result;
    }
    result.errors.Reserve(static_cast<int32>(validation.error().size()));
    for (auto& error : validation.error()) {
        result.errors.Add({.code = error.code, .message = to_fstring(error.message)});
    }
    return result;
}
} // namespace ml
