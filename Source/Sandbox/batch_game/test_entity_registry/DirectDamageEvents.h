#pragma once

#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>

#include <SandboxCore/soa_array_mixin.h>

#include <Containers/Array.h>
#include <HAL/Platform.h>

struct DirectDamageEvents : public ml::FSoAArrayMixin {
    TArray<FRegistryEntityHandle> damaged_entities;
    TArray<int32> damage_amounts;
    TArray<FRegistryEntityHandle> instigators;

#define SANDBOX_PACK(STAMPER, NON_FINAL) \
    NON_FINAL(STAMPER(damaged_entities)) \
    NON_FINAL(STAMPER(damage_amounts))   \
    STAMPER(instigators)

    SANDBOX_SOA_MAKE_APPLY_FNS(SANDBOX_PACK)
#undef SANDBOX_PACK
};
