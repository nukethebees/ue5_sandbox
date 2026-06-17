#pragma once

#include <Sandbox/misc/learning/test_entity_registry/RegistryEntityHandle.h>
#include <Sandbox/misc/learning/test_entity_registry/TestDeathReason.h>

#include <Containers/Array.h>
#include <HAL/Platform.h>

struct EntityDeathInfo {
    TArray<ETestDeathReason> reasons;
    TArray<FRegistryEntityHandle> victims;
    TArray<FRegistryEntityHandle> killers;

    void add(ETestDeathReason const reason,
             FRegistryEntityHandle const victim,
             FRegistryEntityHandle const killer);
    void add(ETestDeathReason const reason, FRegistryEntityHandle const victim) {
        add(reason, victim, FRegistryEntityHandle{});
    }

    auto num() const -> int32;
    void reset();

    void validate_array_sizes() const;
};
