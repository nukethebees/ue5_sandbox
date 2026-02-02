#pragma once

#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include "Sandbox/utilities/macros/create_forwarding_fn.hpp"

class UCollisionEffectSubsystemMixins {
  public:
    FORWARDING_FN(core_, add_payload, &self);
    FORWARDING_FN(core_, register_actor, &self)
  protected:
    FORWARDING_FN(core_, handle_collision_event_)
};

template <typename SubsystemT, typename PayloadT>
inline void try_add_subsystem_payload(AActor& actor, PayloadT&& payload) {
    if (auto* world{actor.GetWorld()}) {
        if (auto* subsystem{world->GetSubsystem<SubsystemT>()}) {
            subsystem->add_payload(actor, std::forward<PayloadT>(payload));
        }
    }
}

template <typename SubsystemT, typename PayloadT, typename... Args>
inline void try_emplace_subsystem_payload(AActor& actor, Args&&... args) {
    if (auto* world{actor.GetWorld()}) {
        if (auto* subsystem{world->GetSubsystem<SubsystemT>()}) {
            subsystem->add_payload(actor, PayloadT{std::forward<Args>(args)...});
        }
    }
}

#include "Sandbox/utilities/macros/create_forwarding_fn_undef.hpp"
