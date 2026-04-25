#pragma once

#include "CoreMinimal.h"

#include "Bool3.generated.h"

USTRUCT()
struct FBool3 {
    GENERATED_BODY()

    bool x{true};
    bool y{true};
    bool z{true};
};

USTRUCT()
struct FRotationBool {
    GENERATED_BODY()

    bool pitch{true};
    bool yaw{true};
    bool roll{true};
};
