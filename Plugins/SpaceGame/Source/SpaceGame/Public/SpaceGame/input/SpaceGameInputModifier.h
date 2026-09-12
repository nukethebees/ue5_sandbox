#pragma once

#include "InputModifiers.h"

#include "SpaceGameInputModifier.generated.h"

namespace ml::ioj {

UENUM()
enum class ESpaceGameInputResponse : uint8 {
    TurnPointerDelta,
    GamepadTurn,
    GamepadMove,
};

UCLASS(NotBlueprintable, meta = (DisplayName = "Space Game Input Response"))
class SPACEGAME_API USpaceGameInputModifier final : public UInputModifier {
    GENERATED_BODY()
  public:
    UPROPERTY(EditAnywhere, Category = "Settings")
    ESpaceGameInputResponse response{ESpaceGameInputResponse::GamepadMove};
  protected:
    auto ModifyRaw_Implementation(UEnhancedPlayerInput const* player_input,
                                  FInputActionValue current_value,
                                  float delta_time) -> FInputActionValue override;
};

} // namespace ml::ioj
