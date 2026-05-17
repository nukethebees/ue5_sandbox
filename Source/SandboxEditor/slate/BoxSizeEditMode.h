#pragma once

#include "CoreMinimal.h"

enum class EBoxSizeEditMode : uint8 {
    xyz,
    uniform,
    xy_and_z,
};

namespace box_size_edit_mode {
auto to_name(EBoxSizeEditMode mode) -> FName;
auto to_text(EBoxSizeEditMode mode) -> FText;
auto from_name(FName name) -> TOptional<EBoxSizeEditMode>;
auto get_names() -> TArray<FName>;
}
