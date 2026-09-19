#include "SandboxEditor/levels/S7LevelAuthoringActors.h"

#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

namespace ml::editor {
auto resolve_s7_level_actor(AActor const& actor) -> TOptional<FS7LevelResolvedActor> {
    EResolvedLevelArchetype archetype{};
    ETestTeam team{};
    if (auto const* const player{Cast<ATestSpaceShip>(&actor)}) {
        archetype = EResolvedLevelArchetype::PlayerFighter;
        team = player->get_team();
    } else if (auto const* const capital{Cast<ATestCapitalShipProxy>(&actor)}) {
        archetype = EResolvedLevelArchetype::CapitalShip;
        team = capital->get_team();
    } else if (auto const* const turret{Cast<ATestStaticTurretsProxy>(&actor)}) {
        archetype = EResolvedLevelArchetype::StaticTurret;
        team = turret->get_team();
    } else {
        return NullOpt;
    }

    auto const level_team{to_level_team_id(team)};
    return level_team.IsSet() ? TOptional<FS7LevelResolvedActor>{FS7LevelResolvedActor{
                                    archetype, level_team.GetValue()}}
                              : NullOpt;
}

auto s7_level_actor_class(EResolvedLevelArchetype const archetype,
                          USpaceGameLevelConfig const& config) -> UClass* {
    switch (archetype) {
        case EResolvedLevelArchetype::PlayerFighter:
            return config.classes.player_ship_class.Get();
        case EResolvedLevelArchetype::CapitalShip:
            return config.classes.capital_ship_proxy_class.Get();
        case EResolvedLevelArchetype::StaticTurret:
            return config.classes.static_turret_proxy_class.Get();
    }
    return nullptr;
}

void configure_s7_level_actor(AActor& actor,
                              EResolvedLevelArchetype const archetype,
                              ETestTeam const team,
                              USpaceGameLevelConfig& config,
                              FTransform const& transform,
                              FName const label) {
    actor.Modify();
    actor.SetActorTransform(transform);
    if (archetype == EResolvedLevelArchetype::PlayerFighter) {
        auto& player{*CastChecked<ATestSpaceShip>(&actor)};
        player.set_actor_config(&config.player_ship);
        player.set_team(team);
    } else if (archetype == EResolvedLevelArchetype::CapitalShip) {
        auto& capital{*CastChecked<ATestCapitalShipProxy>(&actor)};
        capital.set_level_config_asset(&config);
        capital.set_team(team);
    } else {
        auto& turret{*CastChecked<ATestStaticTurretsProxy>(&actor)};
        turret.set_actor_config(&config.turrets);
        turret.set_team(team);
    }
    actor.RerunConstructionScripts();
    actor.PostEditMove(true);
    actor.SetActorLabel(label.ToString(), true);
}

auto canonical_s7_level_entity_id(FStringView const text, FStringView const fallback) -> FName {
    FString result;
    for (auto const character : text) {
        auto const alpha{FChar::IsAlpha(character)};
        auto const digit{FChar::IsDigit(character)};
        if (!alpha && !digit) {
            if (!result.IsEmpty() && !result.EndsWith(TEXT("-"))) {
                result.AppendChar(TEXT('-'));
            }
            continue;
        }
        result.AppendChar(FChar::ToLower(character));
    }
    while (result.EndsWith(TEXT("-"))) {
        result.LeftChopInline(1, EAllowShrinking::No);
    }
    if (result.IsEmpty()) {
        result = fallback;
    }
    if (!FChar::IsAlpha(result[0])) {
        result = FString{fallback} + TEXT("-") + result;
    }
    return FName{result};
}

auto is_canonical_s7_level_entity_id(FStringView const value) -> bool {
    if (value.IsEmpty() || value[0] < TEXT('a') || value[0] > TEXT('z')) {
        return false;
    }
    for (auto const character : value) {
        auto const lower{character >= TEXT('a') && character <= TEXT('z')};
        auto const digit{character >= TEXT('0') && character <= TEXT('9')};
        if (!lower && !digit && character != TEXT('-')) {
            return false;
        }
    }
    return true;
}

auto is_canonical_s7_level_entity_id(FName const id) -> bool {
    return is_canonical_s7_level_entity_id(FStringView{id.ToString()});
}
}
