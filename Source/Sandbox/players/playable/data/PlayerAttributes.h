#pragma once

#include "CoreMinimal.h"

#include "PlayerAttributes.generated.h"

USTRUCT(BlueprintType)
struct FPlayerAttributes {
    GENERATED_BODY()

    FPlayerAttributes() = default;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
    int32 strength{1};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
    int32 endurance{1};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
    int32 agility{1};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
    int32 cyber{1};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
    int32 psi{1};
};
