#include "SpaceGame/levels/LevelDefinition.h"

#include "LevelArchetypeResolution.h"
#include "LevelEntityTableOperations.h"
#include "LevelTeamResolution.h"

#include <Containers/Map.h>
#include <Containers/Set.h>

namespace ml {
namespace {
auto placement_is_valid(FVector const& position, FRotator const& rotation) -> bool {
    return !position.ContainsNaN() && !rotation.ContainsNaN();
}

void add_error(FLevelValidationResult& result,
               ELevelValidationErrorCode const code,
               FString message) {
    result.errors.Add(FLevelValidationError{.code = code, .message = MoveTemp(message)});
}

auto columns_have_equal_size(FLevelEntityTable const& entities) -> bool {
    auto const count{entities.ids.Num()};
    return entities.archetypes.Num() == count && entities.teams.Num() == count &&
           entities.positions.num() == count && entities.rotations.num() == count;
}

auto entity_owner(FLevelEntityId const id, int32 const index) -> FString {
    return id.is_set() ? FString::Printf(TEXT("Entity '%s'"), *id.value.ToString())
                       : FString::Printf(TEXT("Entity at row %d"), index);
}

void validate_metadata(FLevelDefinition const& definition, FLevelValidationResult& result) {
    if (!definition.metadata.id.is_set()) {
        add_error(result,
                  ELevelValidationErrorCode::MissingLevelId,
                  TEXT("Level definition has no stable id"));
    }
    if (definition.metadata.title.TrimStartAndEnd().IsEmpty()) {
        add_error(
            result, ELevelValidationErrorCode::MissingTitle, TEXT("Level definition has no title"));
    }
}

void validate_unlock_criteria(FLevelDefinition const& definition, FLevelValidationResult& result) {
    TSet<FLevelId> referenced_levels;
    for (auto const& criterion : definition.unlock_criteria) {
        auto const& completed{criterion.Get<FLevelCompletedUnlockCriterion>()};
        if (!completed.level_id.is_set()) {
            add_error(result,
                      ELevelValidationErrorCode::MissingUnlockLevelId,
                      TEXT("Level-completed unlock criterion has no level id"));
            continue;
        }
        if (completed.level_id == definition.metadata.id) {
            add_error(result,
                      ELevelValidationErrorCode::SelfUnlockDependency,
                      FString::Printf(TEXT("Level '%s' requires itself to be completed"),
                                      *completed.level_id.value.ToString()));
        }
        if (referenced_levels.Contains(completed.level_id)) {
            add_error(result,
                      ELevelValidationErrorCode::DuplicateUnlockCriterion,
                      FString::Printf(TEXT("Level-completed criterion for '%s' is duplicated"),
                                      *completed.level_id.value.ToString()));
            continue;
        }
        referenced_levels.Add(completed.level_id);
    }
}

auto validate_teams(FLevelDefinition const& definition, FLevelValidationResult& result)
    -> TSet<FLevelTeamId> {
    TSet<FLevelTeamId> declared_teams;
    auto const team_count{definition.teams.Num()};
    declared_teams.Reserve(team_count);
    for (int32 i{0}; i < team_count; ++i) {
        auto const team{definition.teams[i]};
        if (team.value.IsNone()) {
            add_error(result,
                      ELevelValidationErrorCode::EmptyTeamId,
                      FString::Printf(TEXT("Team %d has an empty id"), i));
            continue;
        }
        if (declared_teams.Contains(team)) {
            add_error(result,
                      ELevelValidationErrorCode::DuplicateTeamId,
                      FString::Printf(
                          TEXT("Team %d duplicates team id '%s'"), i, *team.value.ToString()));
            continue;
        }

        declared_teams.Add(team);
        if (!level_team_detail::resolve(team).IsSet()) {
            add_error(result,
                      ELevelValidationErrorCode::UnsupportedTeamId,
                      FString::Printf(TEXT("Team %d uses unsupported team id '%s'"),
                                      i,
                                      *team.value.ToString()));
        }
    }
    return declared_teams;
}

void validate_viewpoint_selection(FLevelDefinition const& definition,
                                  FLevelValidationResult& result) {
    auto const has_player{definition.player_entity_id.is_set()};
    auto const has_camera{definition.camera.IsSet()};
    if (!has_player && !has_camera) {
        add_error(result,
                  ELevelValidationErrorCode::MissingViewpoint,
                  TEXT("Level definition has neither a player nor an initial camera"));
    } else if (has_player && has_camera) {
        add_error(result,
                  ELevelValidationErrorCode::ConflictingViewpoints,
                  TEXT("Level definition cannot have both a player and an initial camera"));
    }
}

void validate_archetype(FEntityArchetypeId const archetype,
                        bool const is_player,
                        FString const& owner,
                        FLevelValidationResult& result) {
    if (archetype.value.IsNone()) {
        add_error(result,
                  ELevelValidationErrorCode::EmptyArchetypeId,
                  FString::Printf(TEXT("%s has an empty archetype id"), *owner));
        return;
    }

    auto const resolved{level_archetype_detail::resolve(archetype)};
    if (!resolved.IsSet()) {
        add_error(result,
                  ELevelValidationErrorCode::UnsupportedArchetype,
                  FString::Printf(TEXT("%s uses unsupported archetype '%s'"),
                                  *owner,
                                  *archetype.value.ToString()));
        return;
    }

    auto const is_player_archetype{resolved.GetValue() ==
                                   level_archetype_detail::EResolvedArchetype::PlayerFighter};
    if (is_player != is_player_archetype) {
        add_error(result,
                  ELevelValidationErrorCode::ArchetypeRoleMismatch,
                  is_player ? FString::Printf(TEXT("Player cannot use archetype '%s'"),
                                              *archetype.value.ToString())
                            : FString::Printf(TEXT("%s cannot use the player archetype"), *owner));
    }
}

struct FEntityValidationState {
    TSet<FLevelEntityId> ids{};
    TMap<FLevelEntityId, FLevelTeamId> teams_by_id{};
    bool player_found{false};
};

auto validate_entities(FLevelDefinition const& definition,
                       TSet<FLevelTeamId> const& declared_teams,
                       FLevelValidationResult& result) -> FEntityValidationState {
    FEntityValidationState state;
    auto const has_player{definition.player_entity_id.is_set()};
    auto const entities{definition.entities.get_const_view()};
    auto const entity_count{entities.num()};
    state.ids.Reserve(entity_count);
    state.teams_by_id.Reserve(entity_count);
    for (int32 i{0}; i < entity_count; ++i) {
        auto const entity{level_entity_table_detail::get(entities, i)};
        auto const owner{entity_owner(entity.id, i)};
        auto const is_player{has_player && entity.id == definition.player_entity_id};

        if (entity.id.is_set()) {
            if (state.ids.Contains(entity.id)) {
                add_error(result,
                          ELevelValidationErrorCode::DuplicateEntityId,
                          FString::Printf(TEXT("%s duplicates authored entity id '%s'"),
                                          *owner,
                                          *entity.id.value.ToString()));
            } else {
                state.ids.Add(entity.id);
                state.teams_by_id.Add(entity.id, entity.team);
            }
        }

        state.player_found = state.player_found || is_player;
        if (!declared_teams.Contains(entity.team)) {
            add_error(result,
                      ELevelValidationErrorCode::UnknownTeamReference,
                      FString::Printf(TEXT("%s references undeclared team '%s'"),
                                      *owner,
                                      *entity.team.value.ToString()));
        }

        validate_archetype(entity.archetype, is_player, owner, result);

        if (!placement_is_valid(entity.position, entity.rotation)) {
            add_error(result,
                      ELevelValidationErrorCode::InvalidPlacement,
                      FString::Printf(TEXT("%s has a non-finite position or rotation"), *owner));
        }
    }

    if (has_player && !state.player_found) {
        add_error(result,
                  ELevelValidationErrorCode::PlayerEntityNotFound,
                  FString::Printf(TEXT("Player entity '%s' is not declared"),
                                  *definition.player_entity_id.value.ToString()));
    }
    return state;
}

void validate_camera(FLevelCameraDefinition const& camera,
                     TSet<FLevelEntityId> const& entity_ids,
                     FLevelValidationResult& result) {
    if (camera.target_entity_ids.IsEmpty()) {
        add_error(result,
                  ELevelValidationErrorCode::MissingCameraTarget,
                  TEXT("Initial camera has no target entities"));
    }

    TSet<FLevelEntityId> camera_target_ids;
    auto const target_count{camera.target_entity_ids.Num()};
    camera_target_ids.Reserve(target_count);
    for (int32 i{0}; i < target_count; ++i) {
        auto const id{camera.target_entity_ids[i]};
        if (camera_target_ids.Contains(id)) {
            add_error(result,
                      ELevelValidationErrorCode::DuplicateCameraTarget,
                      FString::Printf(TEXT("Initial camera target %d duplicates entity '%s'"),
                                      i,
                                      *id.value.ToString()));
            continue;
        }

        camera_target_ids.Add(id);
        if (!id.is_set() || !entity_ids.Contains(id)) {
            add_error(result,
                      ELevelValidationErrorCode::CameraTargetNotFound,
                      FString::Printf(TEXT("Initial camera target '%s' is not declared"),
                                      *id.value.ToString()));
        }
    }

    if (!FMath::IsFinite(camera.distance) || camera.distance <= 0.0) {
        add_error(result,
                  ELevelValidationErrorCode::InvalidCameraDistance,
                  TEXT("Initial camera distance must be finite and greater than zero"));
    }
    if (camera.offset_direction.ContainsNaN() || camera.offset_direction.IsNearlyZero()) {
        add_error(result,
                  ELevelValidationErrorCode::InvalidCameraOffsetDirection,
                  TEXT("Initial camera offset direction must be finite and non-zero"));
    }
}

auto validate_mission_references(TConstArrayView<FLevelEntityId> const references,
                                 FStringView const role,
                                 TSet<FLevelEntityId> const& entity_ids,
                                 FLevelValidationResult& result) -> TSet<FLevelEntityId> {
    TSet<FLevelEntityId> validated_ids;
    validated_ids.Reserve(references.Num());
    auto const reference_count{references.Num()};
    for (int32 i{0}; i < reference_count; ++i) {
        auto const id{references[i]};
        if (!id.is_set() || !entity_ids.Contains(id)) {
            add_error(result,
                      ELevelValidationErrorCode::MissionEntityNotFound,
                      FString::Printf(TEXT("Mission %.*s entity '%s' is not declared"),
                                      role.Len(),
                                      role.GetData(),
                                      *id.value.ToString()));
            continue;
        }
        if (validated_ids.Contains(id)) {
            add_error(result,
                      ELevelValidationErrorCode::DuplicateMissionEntityReference,
                      FString::Printf(TEXT("Mission %.*s entity '%s' is duplicated"),
                                      role.Len(),
                                      role.GetData(),
                                      *id.value.ToString()));
            continue;
        }
        validated_ids.Add(id);
    }
    return validated_ids;
}

void validate_mission(FLevelMissionDefinition const& mission,
                      FEntityValidationState const& entities,
                      FLevelValidationResult& result) {
    auto requires_time_limit{false};
    auto uses_kill_count{false};
    switch (mission.mode) {
        case ELevelMissionMode::Unspecified: {
            add_error(result,
                      ELevelValidationErrorCode::MissingMissionMode,
                      TEXT("Mission definition has no mode"));
            break;
        }
        case ELevelMissionMode::SurviveTime: {
            requires_time_limit = true;
            if (mission.must_survive_entity_ids.IsEmpty()) {
                add_error(result,
                          ELevelValidationErrorCode::MissingMissionSurvivors,
                          TEXT("Survive-time mission has no entities that must survive"));
            }
            break;
        }
        case ELevelMissionMode::KillEnemies: {
            uses_kill_count = true;
            break;
        }
        case ELevelMissionMode::KillEnemiesWithinTime: {
            requires_time_limit = true;
            uses_kill_count = true;
            break;
        }
        default: {
            add_error(result,
                      ELevelValidationErrorCode::UnsupportedMissionMode,
                      TEXT("Mission definition uses an unsupported mode"));
            break;
        }
    }

    if (requires_time_limit) {
        if (!mission.time_limit_seconds.IsSet() ||
            !FMath::IsFinite(mission.time_limit_seconds.GetValue()) ||
            mission.time_limit_seconds.GetValue() <= 0.0f) {
            add_error(result,
                      ELevelValidationErrorCode::InvalidMissionTimeLimit,
                      TEXT("Mission time limit must be finite and greater than zero"));
        }
    } else if (mission.time_limit_seconds.IsSet()) {
        add_error(result,
                  ELevelValidationErrorCode::UnexpectedMissionTimeLimit,
                  TEXT("Untimed mission cannot define a time limit"));
    }

    if (uses_kill_count) {
        if (mission.hero_entity_ids.IsEmpty()) {
            add_error(result,
                      ELevelValidationErrorCode::MissingMissionHeroes,
                      TEXT("Kill mission has no hero entities"));
        }
        if (mission.kill_count.IsSet() && mission.kill_count.GetValue() <= 0) {
            add_error(result,
                      ELevelValidationErrorCode::InvalidMissionKillCount,
                      TEXT("Mission kill count must be greater than zero when specified"));
        }
    } else if (mission.kill_count.IsSet()) {
        add_error(result,
                  ELevelValidationErrorCode::UnexpectedMissionKillCount,
                  TEXT("Survive-time mission cannot define a kill count"));
    }

    auto const heroes{validate_mission_references(
        mission.hero_entity_ids, TEXTVIEW("hero"), entities.ids, result)};
    auto const survivors{validate_mission_references(
        mission.must_survive_entity_ids, TEXTVIEW("must-survive"), entities.ids, result)};
    auto const required_kills{validate_mission_references(
        mission.required_kill_entity_ids, TEXTVIEW("required-kill"), entities.ids, result)};

    for (auto const id : required_kills) {
        if (heroes.Contains(id) || survivors.Contains(id)) {
            add_error(result,
                      ELevelValidationErrorCode::ConflictingMissionEntityRoles,
                      FString::Printf(TEXT("Mission entity '%s' cannot be both required to kill "
                                           "and a hero or must-survive entity"),
                                      *id.value.ToString()));
        }
    }

    if (uses_kill_count && !mission.kill_count.IsSet() && !heroes.IsEmpty()) {
        TOptional<FLevelTeamId> hero_team;
        for (auto const id : heroes) {
            auto const* const team{entities.teams_by_id.Find(id)};
            if (!team) {
                continue;
            }
            if (!hero_team.IsSet()) {
                hero_team = *team;
            } else if (hero_team.GetValue() != *team) {
                add_error(result,
                          ELevelValidationErrorCode::AmbiguousAutomaticKillTeams,
                          TEXT("Automatic kill count requires all hero entities to share a team"));
                break;
            }
        }
    }
}
}

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
    validate_metadata(definition, result);
    validate_unlock_criteria(definition, result);

    if (!columns_have_equal_size(definition.entities)) {
        add_error(result,
                  ELevelValidationErrorCode::MismatchedEntityColumns,
                  TEXT("Level entity columns have inconsistent lengths"));
        return result;
    }

    auto const declared_teams{validate_teams(definition, result)};
    validate_viewpoint_selection(definition, result);
    auto const entities{validate_entities(definition, declared_teams, result)};
    if (definition.camera.IsSet()) {
        validate_camera(definition.camera.GetValue(), entities.ids, result);
    }
    if (definition.mission.IsSet()) {
        validate_mission(definition.mission.GetValue(), entities, result);
    }

    return result;
}
}
