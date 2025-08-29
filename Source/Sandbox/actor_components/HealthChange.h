#pragma once

#include "CoreMinimal.h"

#include "HealthChange.generated.h"

UENUM(BlueprintType)
enum class EHealthChangeType : uint8 {
    Healing UMETA(DisplayName = "Healing"),
    Damage UMETA(DisplayName = "Damage")
};

USTRUCT(BlueprintType)
struct FHealthChange {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float value{0.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EHealthChangeType type{EHealthChangeType::Damage};

    FHealthChange() = default;

    FHealthChange(float value, EHealthChangeType change_type)
        : value(value)
        , type(change_type) {}
};
