#pragma once

#include "SandboxEditor/levels/S7AuthoringResult.h"
#include <ioj/levels/entity_archetype.h>
#include <SpaceGame/defences/spinners/TestTubeSpinnerProxy.h>
#include <SpaceGame/defences/turrets/TestStaticTurretsProxy.h>
#include <SpaceGame/ships/capital/TestCapitalShipProxy.h>
#include <SpaceGame/ships/fighter/TestFighterProxy.h>
#include <SpaceGame/ships/player/TestSpaceShip.h>

#include <Engine/Level.h>

#include <algorithm>
#include <array>

namespace ioj::editor {
template <typename T, levels::EntityArchetype Kind>
struct SceneActorGroup {
    using Actor = T;
    static constexpr auto archetype{Kind};

    template <typename Fn>
    auto visit(Fn& fn) const {
        using Result = std::invoke_result_t<Fn&, T&, levels::EntityArchetype>;
        for (auto* actor : actors) {
            if constexpr (std::is_void_v<Result>) {
                fn(*actor, Kind);
            } else if (auto result{fn(*actor, Kind)}; !result) {
                return result;
            }
        }
        return Result();
    }

    TArray<T*> actors;
};

template <typename... Groups>
class SceneActorCollection : private Groups... {
  public:
    static constexpr auto archetypes{std::array{Groups::archetype...}};

    explicit SceneActorCollection(ULevel const& level) {
        for (auto const actor : level.Actors) {
            if (IsValid(actor)) {
                dispatch(*actor, [this]<typename Group>(auto& proxy) {
                    static_cast<Group&>(*this).actors.Add(&proxy);
                });
            }
        }
    }

    template <typename Actor, typename Fn>
    static bool visit_scene_actor(Actor& actor, Fn&& fn) {
        return dispatch(actor, [&fn]<typename Group>(auto& proxy) { fn(proxy, Group::archetype); });
    }

    template <typename Fn>
    static void visit_archetype(levels::EntityArchetype kind, Fn&& fn) {
        auto const match{[&]<typename Group>() {
            if (kind == Group::archetype) {
                fn.template operator()<typename Group::Actor>();
                return true;
            }
            return false;
        }};
        bool const found{(match.template operator()<Groups>() || ...)};
        check(found);
    }

    template <typename Fn>
    auto for_each_scene_actor(Fn&& fn) const {
        if constexpr ((std::is_void_v<decltype(Groups::visit(fn))> && ...)) {
            (Groups::visit(fn), ...);
        } else {
            S7AuthoringResult<void> result{};
            static_cast<void>(((result = Groups::visit(fn)).has_value() && ...));
            return result;
        }
    }

  private:
    template <typename Actor, typename Fn>
    static bool dispatch(Actor& actor, Fn&& fn) {
        auto const match{[&]<typename Group>() {
            if (auto* proxy{Cast<typename Group::Actor>(&actor)}) {
                fn.template operator()<Group>(*proxy);
                return true;
            }
            return false;
        }};
        return (match.template operator()<Groups>() || ...);
    }
};

using SceneActors = SceneActorCollection<
    SceneActorGroup<ATestSpaceShip, levels::EntityArchetype::PlayerFighter>,
    SceneActorGroup<ATestFighterProxy, levels::EntityArchetype::Fighter>,
    SceneActorGroup<ATestCapitalShipProxy, levels::EntityArchetype::CapitalShip>,
    SceneActorGroup<ATestStaticTurretsProxy, levels::EntityArchetype::StaticTurret>,
    SceneActorGroup<ATestTubeSpinnerProxy, levels::EntityArchetype::TubeSpinner>>;

static_assert(std::ranges::is_permutation(SceneActors::archetypes,
                                          EnumTraits<levels::EntityArchetype>::values),
              "Map each scene archetype to exactly one editor actor type.");
}
