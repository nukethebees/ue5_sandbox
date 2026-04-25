#pragma once

#include "CoreMinimal.h"

#include "AlignAxesCheckboxStates.generated.h"

struct FRotationBool;

USTRUCT()
struct FAlignAxesCheckboxStates {
    GENERATED_BODY()

    UPROPERTY()
    ECheckBoxState pitch{ECheckBoxState::Unchecked};
    UPROPERTY()
    ECheckBoxState yaw{ECheckBoxState::Checked};
    UPROPERTY()
    ECheckBoxState roll{ECheckBoxState::Unchecked};

    auto to_bools() const -> FRotationBool;
};
