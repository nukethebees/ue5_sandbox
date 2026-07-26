#pragma once

#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>
#include <Sandbox/batch_game/TestTeam.h>

#include <SandboxCore/soa_array_mixin.h>
#include <SandboxCore/soa_rotators.h>
#include <SandboxCore/soa_vectors.h>

#include "CoreMinimal.h"

struct TestCapitalShipFighterSpawnQueue : public ml::FSoAArrayMixin {
    FVectors3f locations;
    FRotatorsf rotations;
    TArray<ETestTeam> teams;
    TArray<FRegistryEntityHandle> targets;

#define SANDBOX_PACK(STAMPER, NON_FINAL) \
    NON_FINAL(STAMPER(locations))        \
    NON_FINAL(STAMPER(rotations))        \
    NON_FINAL(STAMPER(teams))            \
    STAMPER(targets)

    SANDBOX_SOA_MAKE_APPLY_FNS(SANDBOX_PACK)
#undef SANDBOX_PACK
};
