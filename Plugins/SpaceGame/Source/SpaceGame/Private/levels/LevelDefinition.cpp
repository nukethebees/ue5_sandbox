#include "SpaceGame/levels/LevelDefinition.h"

#include <SpaceGame/levels/NativeLevelDefinitionConversion.h>
#include "LevelEntityTableOperations.h"

#include <Containers/StringConv.h>

namespace ml {
namespace {
auto columns_have_equal_size(FLevelEntityTable const& entities) -> bool {
    auto const count{entities.ids.Num()};
    return entities.archetypes.Num() == count && entities.teams.Num() == count &&
           entities.positions.num() == count && entities.rotations.num() == count &&
           entities.spawn_times_seconds.Num() == count;
}

auto to_validation_fstring(std::string const& value) -> FString {
    auto const converted{FUTF8ToTCHAR{value.data(), static_cast<int32>(value.size())}};
    return FString{converted.Length(), converted.Get()};
}
} // namespace

void FLevelBuilder::set_metadata(FLevelMetadata const& metadata) {
    definition_.metadata = metadata;
}

void FLevelBuilder::set_player_entity(FLevelEntityId const id) {
    definition_.player_entity_id = id;
}

void FLevelBuilder::set_camera(FLevelCameraDefinition const& camera) {
    definition_.camera = camera;
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

auto FLevelBuilder::add_team(FLevelTeamId const team) -> FLevelTeamId {
    definition_.teams.Add(team);
    return team;
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
    if (!columns_have_equal_size(definition.entities)) {
        result.errors.Add({.code = ELevelValidationErrorCode::MismatchedEntityColumns,
                           .message = TEXT("Level entity columns have inconsistent lengths")});
        return result;
    }

    auto native_result{::ioj::sim::levels::validate_level(level_authoring::to_native(definition))};
    result.errors.Reserve(static_cast<int32>(native_result.errors.size()));
    for (auto& error : native_result.errors) {
        result.errors.Add({.code = error.code, .message = to_validation_fstring(error.message)});
    }
    return result;
}
} // namespace ml
