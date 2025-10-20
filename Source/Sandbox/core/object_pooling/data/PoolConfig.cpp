#include "Sandbox/core/object_pooling/data/PoolConfig.h"

#include "Sandbox/combat/projectiles/actors/BulletActor.h"

TSubclassOf<ABulletActor> FBulletPoolConfig::GetDefaultClass() {
    return ABulletActor::StaticClass();
}
