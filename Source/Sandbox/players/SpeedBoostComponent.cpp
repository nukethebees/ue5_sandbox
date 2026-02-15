#include "Sandbox/players/SpeedBoostComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Sandbox/players/MaxSpeedChangeListener.h"
#include "Sandbox/players/MovementMultiplierReceiver.h"
#include "TimerManager.h"

USpeedBoostComponent::USpeedBoostComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void USpeedBoostComponent::apply_speed_boost(FSpeedBoost boost) {
    auto* const multiplier_receiver{get_multiplier_receiver()};
    if (!multiplier_receiver) {
        return;
    }

    auto const current_time{static_cast<double>(GetWorld()->GetTimeSeconds())};
    auto const expiration{current_time + boost.duration};

    active_boosts.Add(boost);
    total_multiplier *= boost.multiplier;

    // Apply the new total multiplier
    multiplier_receiver->set_movement_multiplier(total_multiplier);

    FTimerHandle handle{};

    GetWorld()->GetTimerManager().SetTimer(
        handle,
        [this, boost]() {
            active_boosts.RemoveSingle(boost);
            total_multiplier /= boost.multiplier;

            auto* const multiplier_receiver{get_multiplier_receiver()};
            if (!multiplier_receiver) {
                return;
            }

            if (active_boosts.IsEmpty()) {
                total_multiplier = 1.0f;
            }

            // Apply the updated multiplier
            multiplier_receiver->set_movement_multiplier(total_multiplier);
        },
        expiration,
        false);
}
