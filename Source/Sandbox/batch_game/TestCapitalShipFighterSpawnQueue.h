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

    template <typename TFunc>
    auto apply_arrays(this auto&& self, TFunc&& func) -> decltype(auto) {
        return std::forward<TFunc>(func)(self.locations, self.rotations, self.teams, self.targets);
    }
};
