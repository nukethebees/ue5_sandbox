#pragma once

#include <Sandbox/misc/learning/test_entity_registry/RegistryEntityHandle.h>
#include <Sandbox/misc/learning/test_entity_registry/TestDeathReason.h>
#include <Sandbox/misc/learning/test_entity_registry/TestEntityUniqueId.h>

#include <Containers/Array.h>
#include <HAL/Platform.h>

struct TestEntityUniqueEntityData {
    using kills_type = uint32;

    TArray<FRegistryEntityHandle> registry_handles;
    TArray<kills_type> kills;
    TArray<uint8> alive;
    TArray<TestEntityUniqueId> killed_by;
    TArray<ETestDeathReason> death_reason;

    auto num() const -> int32;
    void reset();
    void add_defaulted(int32 const count);

    void validate_array_sizes() const;
};
