#pragma once

#include "InputModifiers.h"

#include "SpaceGameInputModifier.generated.h"

namespace ioj {

UENUM()
enum class ESpaceGameInputResponse : uint8 {
    TurnPointerDelta,
    GamepadTurn,
};

UENUM()
enum class ESpaceGameInputAxis : uint8 {
    None,
    Pitch,
    Yaw,
};

UCLASS(NotBlueprintable, meta = (DisplayName = "Space Game Input Response"))
class SPACEGAME_API USpaceGameInputModifier final : public UInputModifier {
    GENERATED_BODY()
  public:
    UPROPERTY(EditAnywhere, Category = "Settings")
    ESpaceGameInputResponse response{ESpaceGameInputResponse::GamepadTurn};

    UPROPERTY(EditAnywhere, Category = "Settings")
    ESpaceGameInputAxis axis{ESpaceGameInputAxis::None};
  protected:
    auto ModifyRaw_Implementation(UEnhancedPlayerInput const* player_input,
                                  FInputActionValue current_value,
                                  float delta_time) -> FInputActionValue override;
};

} // namespace ioj
