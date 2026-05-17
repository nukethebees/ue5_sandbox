#include "BoxSizeEditMode.h"

#include "CoreMinimal.h"

namespace box_size_edit_mode {
auto to_name(EBoxSizeEditMode mode) -> FName {
    switch (mode) {
        case EBoxSizeEditMode::xyz:
            return TEXT("xyz");
        case EBoxSizeEditMode::uniform:
            return TEXT("uniform");
        case EBoxSizeEditMode::xy_and_z:
            return TEXT("xy_and_z");
    }

    return TEXT("???");
}
auto to_text(EBoxSizeEditMode mode) -> FText {
    return FText::FromName(to_name(mode));
}
auto from_name(FName name) -> TOptional<EBoxSizeEditMode> {
    if (name == TEXT("xyz")) {
        return EBoxSizeEditMode::xyz;
    } else if (name == TEXT("uniform")) {
        return EBoxSizeEditMode::uniform;
    } else if (name == TEXT("xy_and_z")) {
        return EBoxSizeEditMode::xy_and_z;
    }

    return {};
}
auto get_names() -> TArray<FName> {
    return {
        "xyz",
        "uniform",
        "xy_and_z",
    };
}
}
