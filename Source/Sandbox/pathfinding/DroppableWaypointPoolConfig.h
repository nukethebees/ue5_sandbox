#pragma once

#include "Sandbox/core/object_pooling/PoolConfig.h"
#include "Sandbox/pathfinding/DroppableWaypoint.h"

#include <optional>

#include "CoreMinimal.h"

using FDroppableWaypointPoolConfig = FPoolConfig<ADroppableWaypoint, 50>;
