#pragma once

#include <SandboxNative/RegistryEntityHandle.h>
#include <SpaceGameSimulation/entities/TestEntityUniqueId.h>

#include <Containers/Map.h>

class AActor;

struct FRegistryEntityIdentifiers {
    FRegistryEntityHandle handle;
    TestEntityUniqueId unique_id;
};

using FProxyEntityMap = TMap<AActor const*, FRegistryEntityIdentifiers>;
