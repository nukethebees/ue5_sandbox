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

void validate_team_reference(FLevelValidationResult& result,
                             TSet<FLevelTeamId> const& declared_teams,
                             FLevelTeamId const team,
                             FString const& owner) {
    if (!declared_teams.Contains(team)) {
        add_error(result,
                  ELevelValidationErrorCode::UnknownTeamReference,
                  FString::Printf(
                      TEXT("%s references undeclared team '%s'"), *owner, *team.value.ToString()));
    }
}

void validate_entity_id(FLevelValidationResult& result,
                        TSet<FLevelEntityId>& entity_ids,
                        FLevelEntityId const id,
                        FString const& owner) {
    if (!id.is_set()) {
        return;
    }

    if (entity_ids.Contains(id)) {
        add_error(result,
                  ELevelValidationErrorCode::DuplicateEntityId,
                  FString::Printf(
                      TEXT("%s duplicates authored entity id '%s'"), *owner, *id.value.ToString()));
        return;
    }

    entity_ids.Add(id);
}
}

void FLevelBuilder::set_player(FPlayerDefinition const& player) {
    definition_.player = player;
}

auto FLevelBuilder::add_team(FTeamDefinition const& team) -> FLevelTeamId {
    definition_.teams.Add(team);
    return team.id;
}

auto FLevelBuilder::add_entity(FEntitySpawnDefinition const& entity) -> FLevelEntityId {
    definition_.entities.Add(entity);
    return entity.id;
}

auto FLevelBuilder::finish() -> FLevelDefinition {
    auto definition{MoveTemp(definition_)};
    definition_ = FLevelDefinition{};
    return definition;
}

auto validate_level(FLevelDefinition const& definition) -> FLevelValidationResult {
    FLevelValidationResult result;
    TSet<FLevelTeamId> declared_teams;

    auto const team_count{definition.teams.Num()};
    declared_teams.Reserve(team_count);
    for (int32 i{0}; i < team_count; ++i) {
        auto const team{definition.teams[i].id};
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

    TSet<FLevelEntityId> entity_ids;
    entity_ids.Reserve(definition.entities.Num() + 1);

    if (!definition.player.IsSet()) {
        add_error(result,
                  ELevelValidationErrorCode::MissingPlayer,
                  TEXT("Level definition has no player"));
    } else {
        auto const& player{definition.player.GetValue()};
        validate_entity_id(result, entity_ids, player.id, TEXT("Player"));
        validate_team_reference(result, declared_teams, player.team, TEXT("Player"));

        if (player.archetype.value.IsNone()) {
            add_error(result,
                      ELevelValidationErrorCode::EmptyArchetypeId,
                      TEXT("Player has an empty archetype id"));
        } else if (player.archetype != level_archetypes::player_fighter) {
            add_error(result,
                      player.archetype == level_archetypes::capital_ship ||
                              player.archetype == level_archetypes::static_turret
                          ? ELevelValidationErrorCode::ArchetypeRoleMismatch
                          : ELevelValidationErrorCode::UnsupportedArchetype,
                      FString::Printf(TEXT("Player cannot use archetype '%s'"),
                                      *player.archetype.value.ToString()));
        }

        if (!placement_is_valid(player.position, player.rotation)) {
            add_error(result,
                      ELevelValidationErrorCode::InvalidPlacement,
                      TEXT("Player has a non-finite position or rotation"));
        }
    }

    auto const entity_count{definition.entities.Num()};
    for (int32 i{0}; i < entity_count; ++i) {
        auto const& entity{definition.entities[i]};
        auto const owner{FString::Printf(TEXT("Entity %d"), i)};
        validate_entity_id(result, entity_ids, entity.id, owner);
        validate_team_reference(result, declared_teams, entity.team, owner);

        if (entity.archetype.value.IsNone()) {
            add_error(result,
                      ELevelValidationErrorCode::EmptyArchetypeId,
                      FString::Printf(TEXT("%s has an empty archetype id"), *owner));
        } else if (entity.archetype == level_archetypes::player_fighter) {
            add_error(result,
                      ELevelValidationErrorCode::ArchetypeRoleMismatch,
                      FString::Printf(TEXT("%s cannot use the player archetype"), *owner));
        } else if (entity.archetype != level_archetypes::capital_ship &&
                   entity.archetype != level_archetypes::static_turret) {
            add_error(result,
                      ELevelValidationErrorCode::UnsupportedArchetype,
                      FString::Printf(TEXT("%s uses unsupported archetype '%s'"),
                                      *owner,
                                      *entity.archetype.value.ToString()));
        }

        if (!placement_is_valid(entity.position, entity.rotation)) {
            add_error(result,
                      ELevelValidationErrorCode::InvalidPlacement,
                      FString::Printf(TEXT("%s has a non-finite position or rotation"), *owner));
        }
    }

    return result;
}
}
