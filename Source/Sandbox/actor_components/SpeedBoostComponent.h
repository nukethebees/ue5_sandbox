#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Sandbox/data/SpeedBoost.h"
#include "Sandbox/interfaces/MovementMultiplierReceiver.h"

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
    float total_multiplier{1.0f};
    FTimerHandle boost_timer_handle{};

    auto get_multiplier_receiver() -> IMovementMultiplierReceiver* {
        auto* const owner{GetOwner()};
        if (!owner) {
            return nullptr;
        }

        return Cast<IMovementMultiplierReceiver>(owner);
    }
};
