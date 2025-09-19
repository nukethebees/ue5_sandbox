#pragma once

#include "CoreMinimal.h"
#include "Sandbox/data/SpeedBoost.h"
#include "Sandbox/characters/MyCharacter.h"

#include "CollisionPayloads.generated.h"

USTRUCT(BlueprintType)
struct FSpeedBoostPayload {
    GENERATED_BODY()

    FSpeedBoostPayload() = default;
    FSpeedBoostPayload(FSpeedBoost boost)
        : speed_boost(boost) {}

    void execute(AActor* actor) {}

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FSpeedBoost speed_boost{};
};

USTRUCT(BlueprintType)
struct FJumpIncreasePayload {
    GENERATED_BODY()

    FJumpIncreasePayload() = default;
    FJumpIncreasePayload(int32 inc)
        : jump_count_increase(inc) {}

    void execute(AActor* actor) {
        UE_LOGFMT(LogTemp, Verbose, "FJumpIncreasePayload::execute");
        if (auto* character{Cast<AMyCharacter>(actor)}) {
            character->increase_max_jump_count(jump_count_increase);
        } else {
            UE_LOGFMT(
                LogTemp, Warning, "FJumpIncreasePayload: Could not cast the actor to character.");
        }
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 jump_count_increase{1};
};
