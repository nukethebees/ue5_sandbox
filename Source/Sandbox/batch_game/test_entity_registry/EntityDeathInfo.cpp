#include "EntityDeathInfo.h"

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>


void EntityDeathInfo::add(ETestDeathReason const reason,
                          FRegistryEntityHandle const victim,
                          FRegistryEntityHandle const killer) {
    reasons.Add(reason);
    victims.Add(victim);
    killers.Add(killer);
}

