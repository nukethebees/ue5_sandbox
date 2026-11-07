#include "SandboxEditor/levels/S7LevelAuthoringActors.h"

#include <ioj/levels/authoring/identifiers.h>
#include <SpaceGame/defences/spinners/TestTubeSpinnerProxy.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <SandboxCoreEngine/strings.h>

#include <algorithm>

namespace ioj::editor {

auto resolve_s7_level_actor(AActor const& actor) -> TOptional<FS7LevelResolvedActor> {
    levels::EntityArchetype archetype{};
    ETestTeam team{};
    double spawn_time_seconds{};
    if (auto const* const player{Cast<ATestSpaceShip>(&actor)}) {
        archetype = levels::EntityArchetype::PlayerFighter;
        team = player->get_team();
    } else if (auto const* const capital{Cast<ATestCapitalShipProxy>(&actor)}) {
        archetype = levels::EntityArchetype::CapitalShip;
        team = capital->get_team();
        spawn_time_seconds = capital->spawn_time_seconds;
    } else if (auto const* const turret{Cast<ATestStaticTurretsProxy>(&actor)}) {
        archetype = levels::EntityArchetype::StaticTurret;
        team = turret->get_team();
        spawn_time_seconds = turret->spawn_time_seconds;
    } else if (auto const* const spinner{Cast<ATestTubeSpinnerProxy>(&actor)}) {
        archetype = levels::EntityArchetype::TubeSpinner;
        team = ETestTeam::White;
        spawn_time_seconds = spinner->spawn_time_seconds;
    } else {
        return NullOpt;
    }

    auto const native_team{to_native(team)};
    if (!std::ranges::contains(EnumTraits<Team>::values, native_team)) {
        return NullOpt;
    }
    return FS7LevelResolvedActor{archetype, native_team, spawn_time_seconds};
}

auto s7_level_actor_class(levels::EntityArchetype const archetype,
                          USpaceGameLevelConfig const& config) -> UClass* {
    switch (archetype) {
        case levels::EntityArchetype::PlayerFighter:
            return config.classes.player_ship_class.Get();
        case levels::EntityArchetype::CapitalShip:
            return config.classes.capital_ship_proxy_class.Get();
        case levels::EntityArchetype::StaticTurret:
            return config.classes.static_turret_proxy_class.Get();
        case levels::EntityArchetype::TubeSpinner:
            return ATestTubeSpinnerProxy::StaticClass();
    }
    return nullptr;
}

void configure_s7_level_actor(AActor& actor,
                              levels::EntityArchetype const archetype,
                              ETestTeam const team,
                              USpaceGameLevelConfig& config,
                              FTransform const& transform,
                              FName const label) {
    actor.Modify();
    actor.SetActorTransform(transform);
    if (archetype == levels::EntityArchetype::PlayerFighter) {
        auto& player{*CastChecked<ATestSpaceShip>(&actor)};
        player.set_actor_config(&config.player_ship);
        player.set_team(team);
    } else if (archetype == levels::EntityArchetype::CapitalShip) {
        auto& capital{*CastChecked<ATestCapitalShipProxy>(&actor)};
        capital.set_level_config_asset(&config);
        capital.set_team(team);
    } else if (archetype == levels::EntityArchetype::StaticTurret) {
        auto& turret{*CastChecked<ATestStaticTurretsProxy>(&actor)};
        turret.set_level_config_asset(&config);
        turret.set_team(team);
    } else {
        CastChecked<ATestTubeSpinnerProxy>(&actor)->set_level_config_asset(&config);
    }
    actor.RerunConstructionScripts();
    actor.PostEditMove(true);
    actor.SetActorLabel(label.ToString(), true);
}

auto canonical_s7_level_entity_id(FStringView const text, FStringView const fallback) -> FName {
    return FName{to_fstring(levels::authoring::canonical_symbol(
        TCHAR_TO_UTF8(*FString{text}), TCHAR_TO_UTF8(*FString{fallback})))};
}

auto is_canonical_s7_level_entity_id(FStringView const value) -> bool {
    return levels::authoring::is_canonical_symbol(TCHAR_TO_UTF8(*FString{value}));
}

}
