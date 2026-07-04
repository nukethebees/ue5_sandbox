#pragma once

#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>
#include <Sandbox/batch_game/test_entity_registry/TestDeathReason.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityUniqueId.h>
#include <Sandbox/batch_game/TestEntityType.h>

#include <SandboxCore/soa_array_mixin.h>

#include <Containers/Array.h>
#include <HAL/Platform.h>

struct SANDBOX_API TestEntityUniqueEntityData : public ml::FSoAArrayMixin {
    using kills_type = uint32;

    TArray<FRegistryEntityHandle::index_type> registry_indices;
    TArray<FRegistryEntityHandle::generation_type> registry_generations;
    TArray<ETestEntityType> entity_types;
    TArray<kills_type> kills;
    TArray<uint8> alive;
    TArray<TestEntityUniqueId> killed_by;
    TArray<ETestDeathReason> death_reason;

    template <typename TFunc>
    auto apply_arrays(this auto&& self, TFunc&& func) -> decltype(auto) {
        return std::forward<TFunc>(func)(self.registry_indices,
                                         self.registry_generations,
                                         self.entity_types,
                                         self.kills,
                                         self.alive,
                                         self.killed_by,
                                         self.death_reason);
    }
};
