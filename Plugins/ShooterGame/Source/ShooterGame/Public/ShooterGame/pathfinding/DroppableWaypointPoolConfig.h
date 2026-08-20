#pragma once

#include "SandboxGameShared/core/object_pooling/PoolConfig.h"
#include "ShooterGame/pathfinding/DroppableWaypoint.h"

#include <optional>

#include "CoreMinimal.h"

using FDroppableWaypointPoolConfig = FPoolConfig<ADroppableWaypoint, 50>;
