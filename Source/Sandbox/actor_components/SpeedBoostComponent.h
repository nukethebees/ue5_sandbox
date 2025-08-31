#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Sandbox/data/SpeedBoost.h"

#include "SpeedBoostComponent.generated.h"

class ACharacter;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API USpeedBoostComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    USpeedBoostComponent();

    void apply_speed_boost(FSpeedBoost boost);
  private:
    TArray<FSpeedBoost> active_boosts{};
    float original_speed{0.0f};
    float total_multiplier{1.0f};
    FTimerHandle boost_timer_handle{};

    auto get_movement() -> UCharacterMovementComponent* {
        auto* const character{Cast<ACharacter>(GetOwner())};
        if (!character) {
            return nullptr;
        }

        return character->GetCharacterMovement();
    }
};
