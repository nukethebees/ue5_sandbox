#pragma once

#include <optional>

#include "CoreMinimal.h"

#include "Sandbox/combat/bullets/BulletActor.h"
#include "Sandbox/core/object_pooling/PoolConfig.h"

using FBulletPoolConfig = FPoolConfig<ABulletActor, 100>;
