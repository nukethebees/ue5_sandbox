#pragma once

#include "CoreMinimal.h"

#include "PlayerTechSkills.generated.h"

USTRUCT(BlueprintType)
struct FPlayerTechSkills {
    GENERATED_BODY()

    FPlayerTechSkills() = default;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
    int32 hacking{1};
};
