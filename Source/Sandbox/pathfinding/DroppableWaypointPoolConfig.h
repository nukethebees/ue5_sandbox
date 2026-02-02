#pragma once

#include <optional>

#include "CoreMinimal.h"

#include "Sandbox/core/object_pooling/data/PoolConfig.h"
#include "Sandbox/pathfinding/DroppableWaypoint.h"

using FDroppableWaypointPoolConfig = FPoolConfig<ADroppableWaypoint, 50>;
