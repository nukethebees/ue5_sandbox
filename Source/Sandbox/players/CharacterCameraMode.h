#pragma once

#include <Sandbox/utilities/enums.h>

#include "CoreMinimal.h"

#include "CharacterCameraMode.generated.h"

UENUM(BlueprintType)
enum class ECharacterCameraMode : uint8 {
    FirstPerson UMETA(DisplayName = "First Person"),
    ThirdPerson UMETA(DisplayName = "Third Person"),
    MAX UMETA(Hidden)
};

namespace ml {
template <>
struct EnumCountTrait<ECharacterCameraMode> {
    static constexpr ECharacterCameraMode count{ECharacterCameraMode::MAX};
    static constexpr auto count_value{std::to_underlying(count)};
};
}
