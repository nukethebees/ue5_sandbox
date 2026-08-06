#pragma once

#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityUniqueId.h>

#include <Containers/Map.h>

class AActor;

struct FRegistryEntityIdentifiers {
    FRegistryEntityHandle handle;
    TestEntityUniqueId unique_id;
};

using FProxyEntityMap = TMap<AActor const*, FRegistryEntityIdentifiers>;
