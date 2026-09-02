#include "SpaceGame/levels/LevelDefinition.h"

#include <Containers/Set.h>

namespace ml {
namespace {
auto is_supported_team(FLevelTeamId const id) -> bool {
    return id == level_teams::white || id == level_teams::red || id == level_teams::green ||
           id == level_teams::blue || id == level_teams::orange || id == level_teams::yellow;
}

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

void append_entity(FLevelEntityTable& entities,
                   FLevelEntityId const id,
                   FEntityArchetypeId const archetype,
                   FLevelTeamId const team,
                   FVector const& position,
                   FRotator const& rotation) {
    entities.ids.Add(id);
    entities.archetypes.Add(archetype);
    entities.teams.Add(team);
    entities.positions.add(position);
    entities.rotations.add(rotation.Pitch, rotation.Yaw, rotation.Roll);
}

void set_entity(FLevelEntityTable& entities,
                int32 const index,
                FLevelEntityId const id,
                FEntityArchetypeId const archetype,
                FLevelTeamId const team,
                FVector const& position,
                FRotator const& rotation) {
    entities.ids[index] = id;
    entities.archetypes[index] = archetype;
    entities.teams[index] = team;
    entities.positions.xs[index] = position.X;
    entities.positions.ys[index] = position.Y;
    entities.positions.zs[index] = position.Z;
    entities.rotations.pitches[index] = rotation.Pitch;
    entities.rotations.yaws[index] = rotation.Yaw;
    entities.rotations.rolls[index] = rotation.Roll;
}

auto entity_owner(FLevelEntityId const id, int32 const index) -> FString {
    return id.is_set() ? FString::Printf(TEXT("Entity '%s'"), *id.value.ToString())
                       : FString::Printf(TEXT("Entity at row %d"), index);
}
}

void FLevelBuilder::set_metadata(FLevelMetadata const& metadata) {
    definition_.metadata = metadata;
}

void FLevelBuilder::set_player_entity(FLevelEntityId const id) {
    definition_.player_entity_id = id;
}

void FLevelBuilder::set_player(FPlayerDefinition const& player) {
    definition_.player_entity_id = player.id;
    if (player_row_index_.IsSet()) {
        set_entity(definition_.entities,
                   player_row_index_.GetValue(),
                   player.id,
                   player.archetype,
                   player.team,
                   player.position,
                   player.rotation);
        return;
    }

    player_row_index_ = definition_.entities.num();
    append_entity(definition_.entities,
                  player.id,
                  player.archetype,
                  player.team,
                  player.position,
                  player.rotation);
}

auto FLevelBuilder::add_team(FTeamDefinition const& team) -> FLevelTeamId {
    definition_.teams.Add(team.id);
    return team.id;
}

auto FLevelBuilder::add_entity(FEntitySpawnDefinition const& entity) -> FLevelEntityId {
    append_entity(definition_.entities,
                  entity.id,
                  entity.archetype,
                  entity.team,
                  entity.position,
                  entity.rotation);
    return entity.id;
}

auto FLevelBuilder::finish() -> FLevelDefinition {
    auto definition{MoveTemp(definition_)};
    definition_ = FLevelDefinition{};
    player_row_index_.Reset();
    return definition;
}

auto validate_level(FLevelDefinition const& definition) -> FLevelValidationResult {
    FLevelValidationResult result;
    if (definition.metadata.title.TrimStartAndEnd().IsEmpty()) {
        add_error(
            result, ELevelValidationErrorCode::MissingTitle, TEXT("Level definition has no title"));
    }

    if (!columns_have_equal_size(definition.entities)) {
        add_error(result,
                  ELevelValidationErrorCode::MismatchedEntityColumns,
                  TEXT("Level entity columns have inconsistent lengths"));
        return result;
    }

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
        if (!is_supported_team(team)) {
            add_error(result,
                      ELevelValidationErrorCode::UnsupportedTeamId,
                      FString::Printf(TEXT("Team %d uses unsupported team id '%s'"),
                                      i,
                                      *team.value.ToString()));
        }
    }

    if (!definition.player_entity_id.is_set()) {
        add_error(result,
                  ELevelValidationErrorCode::MissingPlayer,
                  TEXT("Level definition has no player entity id"));
    }

    TSet<FLevelEntityId> entity_ids;
    auto const entities{definition.entities.get_const_view()};
    auto const entity_count{entities.num()};
    entity_ids.Reserve(entity_count);
    bool player_found{false};
    for (int32 i{0}; i < entity_count; ++i) {
        auto const id{entities.ids[i]};
        auto const archetype{entities.archetypes[i]};
        auto const team{entities.teams[i]};
        auto const owner{entity_owner(id, i)};
        auto const is_player{id == definition.player_entity_id};

        if (id.is_set()) {
            if (entity_ids.Contains(id)) {
                add_error(result,
                          ELevelValidationErrorCode::DuplicateEntityId,
                          FString::Printf(TEXT("%s duplicates authored entity id '%s'"),
                                          *owner,
                                          *id.value.ToString()));
            } else {
                entity_ids.Add(id);
            }
        }

        if (is_player) {
            player_found = true;
        }

        if (!declared_teams.Contains(team)) {
            add_error(result,
                      ELevelValidationErrorCode::UnknownTeamReference,
                      FString::Printf(TEXT("%s references undeclared team '%s'"),
                                      *owner,
                                      *team.value.ToString()));
        }

        if (archetype.value.IsNone()) {
            add_error(result,
                      ELevelValidationErrorCode::EmptyArchetypeId,
                      FString::Printf(TEXT("%s has an empty archetype id"), *owner));
        } else if (is_player && archetype != level_archetypes::player_fighter) {
            add_error(result,
                      archetype == level_archetypes::capital_ship ||
                              archetype == level_archetypes::static_turret
                          ? ELevelValidationErrorCode::ArchetypeRoleMismatch
                          : ELevelValidationErrorCode::UnsupportedArchetype,
                      FString::Printf(TEXT("Player cannot use archetype '%s'"),
                                      *archetype.value.ToString()));
        } else if (!is_player && archetype == level_archetypes::player_fighter) {
            add_error(result,
                      ELevelValidationErrorCode::ArchetypeRoleMismatch,
                      FString::Printf(TEXT("%s cannot use the player archetype"), *owner));
        } else if (archetype != level_archetypes::player_fighter &&
                   archetype != level_archetypes::capital_ship &&
                   archetype != level_archetypes::static_turret) {
            add_error(result,
                      ELevelValidationErrorCode::UnsupportedArchetype,
                      FString::Printf(TEXT("%s uses unsupported archetype '%s'"),
                                      *owner,
                                      *archetype.value.ToString()));
        }

        auto const position{entities.positions[i]};
        auto const rotation{FRotator{entities.rotations.pitches[i],
                                     entities.rotations.yaws[i],
                                     entities.rotations.rolls[i]}};
        if (!placement_is_valid(position, rotation)) {
            add_error(result,
                      ELevelValidationErrorCode::InvalidPlacement,
                      FString::Printf(TEXT("%s has a non-finite position or rotation"), *owner));
        }
    }

    if (definition.player_entity_id.is_set() && !player_found) {
        add_error(result,
                  ELevelValidationErrorCode::PlayerEntityNotFound,
                  FString::Printf(TEXT("Player entity '%s' is not declared"),
                                  *definition.player_entity_id.value.ToString()));
    }

    return result;
}
}
