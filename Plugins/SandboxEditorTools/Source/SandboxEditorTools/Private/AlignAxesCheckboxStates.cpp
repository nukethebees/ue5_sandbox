#include "AlignAxesCheckboxStates.h"

#include "Bool3.h"

auto FAlignAxesCheckboxStates::to_bools() const -> FRotationBool {
    return {.pitch = pitch == ECheckBoxState::Checked,
            .yaw = yaw == ECheckBoxState::Checked,
            .roll = roll == ECheckBoxState::Checked};
}
