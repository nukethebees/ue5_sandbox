#pragma once

#include "CoreMinimal.h"

#include "ForcefieldState.generated.h"

UENUM(BlueprintType)
enum class EForcefieldState : uint8 {
    Inactive UMETA(DisplayName = "Inactive"),
    Activating UMETA(DisplayName = "Activating"),
    Active UMETA(DisplayName = "Active"),
    Deactivating UMETA(DisplayName = "Deactivating"),
    Cooldown UMETA(DisplayName = "Cooldown")
};

inline FString to_fstring(EForcefieldState state) {
    return UEnum::GetValueAsString(state);
}