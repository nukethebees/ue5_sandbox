#include "Sandbox/actor_components/SpeedBoostComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Sandbox/interfaces/MaxSpeedChangeListener.h"
#include "TimerManager.h"

USpeedBoostComponent::USpeedBoostComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void USpeedBoostComponent::apply_speed_boost(FSpeedBoost boost) {
    auto* const movement{get_movement()};
    if (!movement) {
        return;
    }

    auto const current_time{static_cast<double>(GetWorld()->GetTimeSeconds())};
    auto const expiration{current_time + boost.duration};

    if (active_boosts.IsEmpty()) {
        original_speed = movement->MaxWalkSpeed;
    }

    active_boosts.Add(boost);
    total_multiplier *= boost.multiplier;
    auto const new_speed{original_speed * total_multiplier};
    movement->MaxWalkSpeed = new_speed;
    IMaxSpeedChangeListener::try_broadcast(GetOwner(), new_speed);

    FTimerHandle handle{};

    GetWorld()->GetTimerManager().SetTimer(
        handle,
        [this, boost]() {
            active_boosts.RemoveSingle(boost);
            total_multiplier /= boost.multiplier;

            auto* const movement{get_movement()};
            if (!movement) {
                return;
            }

            if (active_boosts.IsEmpty()) {
                movement->MaxWalkSpeed = original_speed;
                IMaxSpeedChangeListener::try_broadcast(GetOwner(), original_speed);
                total_multiplier = 1.0f;
            } else {
                auto const new_speed{original_speed * total_multiplier};
                IMaxSpeedChangeListener::try_broadcast(GetOwner(), new_speed);
                movement->MaxWalkSpeed = new_speed;
            }
        },
        expiration,
        false);
}
