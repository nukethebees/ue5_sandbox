#include "SandboxEditor/levels/S7LevelAuthoringActors.h"

#include "SandboxEditor/levels/S7LevelAuthoringDocument.h"
#include "SandboxEditor/levels/S7LevelAuthoringSession.h"
#include "SandboxEditor/levels/S7SceneActors.h"
#include <ioj/levels/authoring/identifiers.h>
#include <SpaceGame/defences/spinners/TestTubeSpinnerProxy.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/fighter/TestFighterProxy.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

#include <SandboxCoreEngine/strings.h>

#include <Engine/Level.h>

#include <algorithm>
#include <utility>

namespace ioj::editor {

namespace {
bool has_indexed_label(FString const& label, FString const& prefix) {
    if (!label.StartsWith(prefix, ESearchCase::CaseSensitive)) {
        return false;
    }

    auto const suffix{label.Mid(prefix.Len())};
    return !suffix.IsEmpty() && FChar::IsDigit(suffix[0]) &&
           suffix == FString::FromInt(FCString::Atoi(*suffix));
}

void assign_proxy_labels(ULevel const& level, TSet<AActor*> const* selected) {
    SceneActors const scene{level};
    TMap<FString, AActor*> bound_ids;
    auto const document{find_level_authoring_document(level)};
    if (document && *document) {
        for (auto const& binding : (*document)->entities) {
            if (IsValid(binding.actor)) {
                bound_ids.Add(binding.id, binding.actor.Get());
            }
        }
    }

    TSet<FString> occupied;
    struct Candidate {
        AActor* actor;
        FString prefix;
    };
    TArray<Candidate> pending;
    scene.for_each_scene_actor([&](auto& actor, levels::EntityArchetype archetype) {
        if constexpr (requires { actor.custom_name; }) {
            if (!selected || selected->Contains(&actor)) {
                if (actor.custom_name.IsNone()) {
                    auto const team{StaticEnum<ETestTeam>()->GetNameStringByValue(
                        static_cast<int64>(actor.get_team()))};
                    auto prefix{to_fstring(levels::to_string(archetype)) + TEXT("-") + team +
                                TEXT("-")};
                    pending.Add(Candidate{&actor, prefix.ToLower()});
                    return;
                }
                actor.SetActorLabel(actor.custom_name.ToString());
            }
        }
        occupied.Add(
            canonical_s7_level_entity_id(actor.GetActorLabel(), TEXTVIEW("entity")).ToString());
    });

    auto const available{[&](FString const& label, AActor* actor) {
        auto const* owner{bound_ids.Find(label)};
        return !occupied.Contains(label) && (!owner || *owner == actor);
    }};
    for (auto& candidate : pending) {
        auto const& current{candidate.actor->GetActorLabel()};
        if (has_indexed_label(current, candidate.prefix) && available(current, candidate.actor)) {
            occupied.Add(current);
            candidate.actor = nullptr;
        }
    }

    TMap<FString, int32> next_indices;
    for (auto const& candidate : pending) {
        if (!candidate.actor) {
            continue;
        }
        auto& index{next_indices.FindOrAdd(candidate.prefix)};
        FString label;
        do {
            label = candidate.prefix + FString::FromInt(index++);
        } while (!available(label, candidate.actor));

        candidate.actor->SetActorLabel(label);
        occupied.Add(label);
    }
}
}

void name_placed_s7_proxies(ULevel const& level, TSet<AActor*> const& placed) {
    assign_proxy_labels(level, &placed);
}
void refresh_s7_proxy_labels(ULevel const& level) {
    assign_proxy_labels(level, nullptr);
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
