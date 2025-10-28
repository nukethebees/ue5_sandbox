#pragma once

#include <optional>

#include "CoreMinimal.h"

#include "Sandbox/combat/projectiles/actors/BulletActor.h"
#include "Sandbox/core/object_pooling/data/PoolConfig.h"

using FBulletPoolConfig = FPoolConfig<ABulletActor, 100>;
