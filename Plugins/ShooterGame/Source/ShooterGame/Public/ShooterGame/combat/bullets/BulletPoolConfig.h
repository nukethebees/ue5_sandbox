#pragma once

#include <optional>

#include "CoreMinimal.h"

#include "ShooterGame/combat/bullets/BulletActor.h"
#include "SandboxGameShared/core/object_pooling/PoolConfig.h"

using FBulletPoolConfig = FPoolConfig<ABulletActor, 100>;
