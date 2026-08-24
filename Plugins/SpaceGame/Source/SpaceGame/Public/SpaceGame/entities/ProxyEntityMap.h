#pragma once

#include <SpaceGame/entities/TestEntityUniqueId.h>
#include <SandboxNative/RegistryEntityHandle.h>

#include <Containers/Map.h>

class AActor;

struct FRegistryEntityIdentifiers {
    FRegistryEntityHandle handle;
    TestEntityUniqueId unique_id;
};

using FProxyEntityMap = TMap<AActor const*, FRegistryEntityIdentifiers>;
