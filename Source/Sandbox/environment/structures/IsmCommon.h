#pragma once

#include "CoreMinimal.h"

#include "IsmCommon.generated.h"

USTRUCT(BlueprintType)
struct FGridAxis {
    GENERATED_BODY()

    constexpr FGridAxis() = default;
    constexpr FGridAxis(int32 p, int32 n)
        : positive(p)
        , negative(n) {}

    static constexpr auto OneVector() -> FGridAxis { return {1, 0}; }

    UPROPERTY(EditAnywhere)
    int32 positive{0};
    UPROPERTY(EditAnywhere)
    int32 negative{0};

    auto num() const { return positive + negative; }
};

UENUM()
enum class ERotationMode : uint8 { parent, from_centre };
