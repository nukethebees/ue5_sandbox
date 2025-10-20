#include "Sandbox/data/pool/PoolConfig.h"

#include "Sandbox/actors/BulletActor.h"

TSubclassOf<ABulletActor> FBulletPoolConfig::GetDefaultClass() {
    return ABulletActor::StaticClass();
}
