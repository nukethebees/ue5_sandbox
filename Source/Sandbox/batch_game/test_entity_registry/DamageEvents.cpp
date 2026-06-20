#include "DamageEvents.h"

#include <SandboxCore/array_checks.h>
#include <SandboxCore/array_utils.h>

auto UnresolvedDamageEvents::num() const -> int32 {
    return damaged_actors.Num();
}

void UnresolvedDamageEvents::reset() {
    damaged_actors.Reset();
    damage_amounts.Reset();
    actor_components.Reset();
    hit_items.Reset();
    instigators.Reset();
}

void UnresolvedDamageEvents::validate_array_sizes() const {
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(damaged_actors),
        SANDBOX_NAMED_NUM(damage_amounts),
        SANDBOX_NAMED_NUM(actor_components),
        SANDBOX_NAMED_NUM(hit_items),
        SANDBOX_NAMED_NUM(instigators),
    });
}

auto DamageEvents::num() const -> int32 {
    return damage_amounts.Num();
}
void DamageEvents::reset() {
    ml::reset(damage_amounts, actor_components, hit_items, instigators);
}
void DamageEvents::remove_at_swap(int32 const index,
                                  int32 const count,
                                  EAllowShrinking const allow_shrinking) {
    damage_amounts.RemoveAtSwap(index, count, allow_shrinking);
    actor_components.RemoveAtSwap(index, count, allow_shrinking);
    hit_items.RemoveAtSwap(index, count, allow_shrinking);
    instigators.RemoveAtSwap(index, count, allow_shrinking);
}
void DamageEvents::validate_array_sizes() const {
    ml::fatal_if_nums_not_equal({
        SANDBOX_NAMED_NUM(damage_amounts),
        SANDBOX_NAMED_NUM(actor_components),
        SANDBOX_NAMED_NUM(hit_items),
        SANDBOX_NAMED_NUM(instigators),
    });
}
