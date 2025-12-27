#pragma once

#include "CoreMinimal.h"

#include "LocalTestEnum.generated.h"

UENUM(BlueprintType)
enum class ELocalTestEnum : uint8 {
    Foo UMETA(DisplayName = "Foo"),
    Bar UMETA(DisplayName = "Bar"),
    Baz
};
