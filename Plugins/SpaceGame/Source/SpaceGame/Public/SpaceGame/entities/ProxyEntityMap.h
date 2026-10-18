#pragma once

#include <ioj/sim/entities/entity_id.h>

#include <Containers/Map.h>

class AActor;

using FProxyEntityMap = TMap<AActor const*, ::ioj::sim::EntityId>;
