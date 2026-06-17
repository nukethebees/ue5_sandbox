#include "EntityDeathInfo.h"

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>

auto EntityDeathInfo::num() const -> int32 {
    return reasons.Num();
}
void EntityDeathInfo::reset() {
    ml::reset(reasons, victims, killers);
}
void EntityDeathInfo::add(ETestDeathReason const reason,
                          FRegistryEntityHandle const victim,
                          FRegistryEntityHandle const killer) {
    reasons.Add(reason);
    victims.Add(victim);
    killers.Add(killer);
}

void EntityDeathInfo::validate_array_sizes() const {
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(reasons),
        SANDBOX_NAMED_NUM(victims),
        SANDBOX_NAMED_NUM(killers),
    });
}
