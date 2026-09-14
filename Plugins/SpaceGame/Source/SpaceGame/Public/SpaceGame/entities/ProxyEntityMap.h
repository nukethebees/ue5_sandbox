#pragma once

#include <ioj/sim/entity_types.h>
#include <SandboxNative/RegistryEntityHandle.h>

#include <Containers/Map.h>

class AActor;

struct FRegistryEntityIdentifiers {
    ::ioj::sim::RegistryEntityHandle handle;
    ::ioj::sim::EntityUniqueId unique_id;
};

using FProxyEntityMap = TMap<AActor const*, FRegistryEntityIdentifiers>;
