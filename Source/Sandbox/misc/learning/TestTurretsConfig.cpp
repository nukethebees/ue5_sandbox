#include "TestTurretsConfig.h"

#include "Sandbox/combat/weapons/ShipLaser.h"

UTestTurretsConfig::UTestTurretsConfig() {
#if WITH_EDITOR
    SetFlags(RF_Transactional);
#endif
}

auto UTestTurretsConfig::is_ready() const noexcept -> bool {
    return (body_mesh != nullptr) && (cannon_mesh != nullptr) && (laser_class != nullptr);
}
