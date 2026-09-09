#pragma once
#include <Containers/StaticArray.h>
#include <CoreMinimal.h>
#include "HudUpdateSettings.generated.h"
USTRUCT(BlueprintType)
struct FTestBatchGameUiUpdateFrequencies {
    GENERATED_BODY()

    [[nodiscard]] auto to_array() const -> TStaticArray<float, 3> {
        return {
            player_status_update_period, entity_count_update_period, mission_status_update_period};
    }

    UPROPERTY(EditAnywhere, Category = "UI")
    float player_status_update_period{0.25f};

    UPROPERTY(EditAnywhere, Category = "UI")
    float entity_count_update_period{0.25f};

    UPROPERTY(EditAnywhere, Category = "UI")
    float mission_status_update_period{0.25f};
};
