#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"

namespace ml::bullet_ids {
using Id = FPrimaryAssetId;
using IdConst = FPrimaryAssetId const;
inline IdConst standard{TEXT("Bullet"), TEXT("Standard")};
}
