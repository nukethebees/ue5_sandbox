#pragma once

#include <CoreMinimal.h>

struct SPACEGAMERENDERING_API FSparkBurstEmission {
    FVector3f location{FVector3f::ZeroVector};
    FVector3f direction{FVector3f::UpVector};
    FVector3f colour{FVector3f::OneVector};
    uint32 seed{0};
};
