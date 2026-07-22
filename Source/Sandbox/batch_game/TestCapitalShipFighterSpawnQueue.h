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

    // clang-format off
#define SANDBOX_PACK(STAMPER, END_SYMBOL)   \
    STAMPER(locations)                      \
    END_SYMBOL STAMPER(rotations)           \
    END_SYMBOL STAMPER(teams)               \
    END_SYMBOL STAMPER(targets)
    // clang-format on

    SANDBOX_SOA_MAKE_APPLY_FNS(SANDBOX_PACK)
#undef SANDBOX_PACK
};
