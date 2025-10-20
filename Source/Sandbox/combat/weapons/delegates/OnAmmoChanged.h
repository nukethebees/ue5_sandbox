#pragma once

#include "Sandbox/combat/weapons/data/AmmoData.h"

#include "OnAmmoChanged.generated.h"

// Shared navigation delegates for menu widgets
UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAmmoChanged, FAmmoData, CurrentAmmo);
