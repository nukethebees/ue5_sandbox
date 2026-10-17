#pragma once

#include <ioj/sim/entities/entity_types.h>

#include <Containers/Map.h>

class AActor;

using FProxyEntityMap = TMap<AActor const*, ::ioj::sim::EntityUniqueId>;
