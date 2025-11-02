#pragma once

#include "CoreMinimal.h"

#include "PlayerTechSkills.generated.h"

USTRUCT(BlueprintType)
struct FPlayerTechSkills {
    GENERATED_BODY()

    FPlayerTechSkills() = default;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
    int32 hacking{1};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
    int32 repair{1};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
    int32 maintenance{1};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
    int32 lockpicking{1};
};
