#include "SandboxEditor/levels/S7LevelAuthoringActors.h"

#include "SandboxEditor/levels/S7SceneActors.h"
#include <ioj/levels/authoring/identifiers.h>
#include <SpaceGame/defences/spinners/TestTubeSpinnerProxy.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/entities/TestProxyActorFunctions.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/fighter/TestFighterProxy.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <SandboxCoreEngine/strings.h>

#include <algorithm>
#include <utility>

namespace ioj::editor {

namespace {
void rename_proxy(ATestFighterProxy& actor, ProxyLabelUpdate update) {
    set_proxy_actor_name(actor, TEXT("Fighter"), actor.get_team(), update);
}
void rename_proxy(ATestCapitalShipProxy& actor, ProxyLabelUpdate update) {
    set_proxy_actor_name(actor, TEXT("CapitalShip"), actor.get_team(), update);
}
void rename_proxy(ATestStaticTurretsProxy& actor, ProxyLabelUpdate update) {
    set_proxy_actor_name(actor, TEXT("StaticTurret"), actor.get_team(), update);
}
void rename_proxy(ATestSpaceShip&, ProxyLabelUpdate) {}
void rename_proxy(ATestTubeSpinnerProxy&, ProxyLabelUpdate) {}
}

void update_s7_proxy_label(AActor& actor, ProxyLabelUpdate update) {
    SceneActors::visit_scene_actor(actor,
                                   [update](auto& proxy, auto) { rename_proxy(proxy, update); });
}
void refresh_s7_proxy_labels(ULevel const& level) {
    SceneActors{level}.for_each_scene_actor(
        [](auto& proxy, auto) { rename_proxy(proxy, ProxyLabelUpdate::Refresh); });
}

auto resolve_s7_level_actor(AActor const& actor) -> TOptional<FS7LevelResolvedActor> {
    TOptional<FS7LevelResolvedActor> result;
    SceneActors::visit_scene_actor(actor, [&](auto const& proxy, auto archetype) {
        using Actor = std::remove_cvref_t<decltype(proxy)>;
        auto team{ETestTeam::White};
        if constexpr (!std::is_same_v<Actor, ATestTubeSpinnerProxy>) {
            team = proxy.get_team();
        }
        auto const native_team{to_native(team)};
        if (!std::ranges::contains(EnumTraits<Team>::values, native_team)) {
            return;
        }
        double spawn_time{};
        if constexpr (!std::is_same_v<Actor, ATestSpaceShip>) {
            spawn_time = proxy.spawn_time_seconds;
        }
        result.Emplace(
            FS7LevelResolvedActor{archetype, native_team, spawn_time, proxy.export_to_script});
    });
    return result;
}

auto s7_level_actor_class(levels::EntityArchetype const archetype,
                          USpaceGameLevelConfig const& config) -> UClass* {
    UClass* type{};
    SceneActors::visit_archetype(archetype, [&]<typename Actor>() {
        if constexpr (std::is_same_v<Actor, ATestSpaceShip>) {
            type = config.classes.player_ship_class.Get();
        } else if constexpr (std::is_same_v<Actor, ATestCapitalShipProxy>) {
            type = config.classes.capital_ship_proxy_class.Get();
        } else if constexpr (std::is_same_v<Actor, ATestStaticTurretsProxy>) {
            type = config.classes.static_turret_proxy_class.Get();
        } else {
            static_assert(std::is_same_v<Actor, ATestFighterProxy> ||
                          std::is_same_v<Actor, ATestTubeSpinnerProxy>);
            type = Actor::StaticClass();
        }
    });
    return type;
}

auto canonical_s7_level_entity_id(FStringView const text, FStringView const fallback) -> FName {
    return FName{to_fstring(levels::authoring::canonical_symbol(
        TCHAR_TO_UTF8(*FString{text}), TCHAR_TO_UTF8(*FString{fallback})))};
}

auto is_canonical_s7_level_entity_id(FStringView const value) -> bool {
    return levels::authoring::is_canonical_symbol(TCHAR_TO_UTF8(*FString{value}));
}

}
