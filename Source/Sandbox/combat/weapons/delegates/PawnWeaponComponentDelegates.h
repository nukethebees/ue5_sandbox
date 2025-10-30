#pragma once

#include "Sandbox/combat/weapons/data/AmmoData.h"

// Shared navigation delegates for menu widgets
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAmmoChanged, FAmmoData /* weapon_ammo */);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnWeaponEquipped,
                                       FAmmoData /* weapon_ammo */,
                                       FAmmoData /* max_ammo */,
                                       FAmmoData /* reserve_ammo */);
DECLARE_MULTICAST_DELEGATE(FOnWeaponUnequipped);
