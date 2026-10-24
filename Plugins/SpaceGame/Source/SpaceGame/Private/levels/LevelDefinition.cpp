#include "SpaceGame/levels/LevelDefinition.h"

#include <SpaceGame/levels/native_level_definition_conversion.h>

#include <SandboxCoreEngine/strings.h>

namespace ioj {
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

void FLevelBuilder::set_mission(levels::MissionDefinition const& mission) {
    definition_.mission = mission;
}

void FLevelBuilder::add_unlock_criterion(FLevelUnlockCriterion criterion) {
    definition_.unlock_criteria.Add(MoveTemp(criterion));
}

auto FLevelBuilder::add_entity(FEntitySpawnDefinition const& entity) -> FLevelEntityId {
    definition_.entities.Add(entity);
    return entity.id;
}

auto FLevelBuilder::finish() -> FLevelDraft {
    auto definition{MoveTemp(definition_)};
    definition_ = FLevelDraft{};
    return definition;
}

auto validate_level(FLevelDraft const& definition) -> FLevelValidationResult {
    FLevelValidationResult result;
    auto native_result{levels::authoring::to_native(definition)};
    if (native_result) {
        return result;
    }
    result.errors.Reserve(static_cast<int32>(native_result.error().size()));
    for (auto& error : native_result.error()) {
        result.errors.Add({.code = error.code, .message = to_fstring(error.message)});
    }
    return result;
}
} // namespace ioj
