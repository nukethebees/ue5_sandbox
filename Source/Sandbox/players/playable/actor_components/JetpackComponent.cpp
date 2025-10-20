#include "Sandbox/players/playable/actor_components/JetpackComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UJetpackComponent::UJetpackComponent() {
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    fuel = fuel_max;
}
void UJetpackComponent::BeginPlay() {
    Super::BeginPlay();
}
void UJetpackComponent::TickComponent(float delta_time,
                                      ELevelTick tick_type,
                                      FActorComponentTickFunction* this_tick_function) {
    Super::TickComponent(delta_time, tick_type, this_tick_function);

    if (is_jetpacking) {
        apply_jetpack_force(delta_time);
    }
}
void UJetpackComponent::start_jetpack() {
    if (fuel <= 0.0f) {
        return;
    }

    auto* world{GetWorld()};
    if (!world) {
        return;
    }
    auto& timer_manager{world->GetTimerManager()};

    is_jetpacking = true;
    SetComponentTickEnabled(true);

    timer_manager.ClearTimer(fuel_recharge_timer);

    if (!timer_manager.IsTimerActive(fuel_broadcast_timer)) {
        constexpr float broadcast_interval{0.1f};
        constexpr bool broadcast_loops{true};

        timer_manager.SetTimer(fuel_broadcast_timer,
                               this,
                               &UJetpackComponent::broadcast_fuel_state,
                               broadcast_interval,
                               broadcast_loops);
    }
}
void UJetpackComponent::stop_jetpack() {
    is_jetpacking = false;

    SetComponentTickEnabled(false);

    if (fuel < fuel_max) {
        constexpr float poll_interval{0.2f};
        constexpr bool loop{true};

        GetWorld()->GetTimerManager().SetTimer(
            recharge_poll_timer, this, &UJetpackComponent::try_start_recharge, poll_interval, loop);
    }
}
void UJetpackComponent::apply_jetpack_force(float delta_time) {
    if (fuel <= 0.0f) {
        return;
    }

    if (auto* character{Cast<ACharacter>(GetOwner())}) {
        auto const fuel_needed{fuel_consumption_rate * delta_time};
        auto const fuel_ratio{FMath::Clamp(fuel / fuel_needed, 0.0f, 1.0f)};

        if (fuel_ratio <= 0.01) {
            return;
        }

        auto const scaled_force{lift_force * delta_time * fuel_ratio};
        if (scaled_force <= 100.0f) {
            return;
        }

        FVector const force{0.0f, 0.0f, scaled_force};
        character->LaunchCharacter(force, false, true);
    }

    fuel -= fuel_consumption_rate * delta_time;
    fuel = FMath::Clamp(fuel, 0.0f, fuel_max);
}
void UJetpackComponent::try_start_recharge() {
    if (auto* character{Cast<ACharacter>(GetOwner())}) {
        if (character->GetCharacterMovement()->IsMovingOnGround()) {
            GetWorld()->GetTimerManager().ClearTimer(recharge_poll_timer);

            constexpr float recharge_interval{0.2f};
            constexpr float recharge_delay{1.0f};
            constexpr bool loop{true};

            GetWorld()->GetTimerManager().SetTimer(fuel_recharge_timer,
                                                   this,
                                                   &UJetpackComponent::recharge_fuel,
                                                   recharge_interval,
                                                   loop,
                                                   recharge_delay);
        }
    }
}
void UJetpackComponent::recharge_fuel() {
    fuel += fuel_recharge_rate;
    fuel = FMath::Clamp(fuel, 0.0f, fuel_max);

    if (fuel >= fuel_max) {
        end_timers();
        broadcast_fuel_state();
    }
}
void UJetpackComponent::broadcast_fuel_state() const {
    FJetpackState const state{fuel, fuel_max, is_jetpacking};
    on_fuel_changed.Broadcast(state);
}
void UJetpackComponent::end_timers() {
    auto& timer_manager{GetWorld()->GetTimerManager()};
    timer_manager.ClearTimer(fuel_recharge_timer);
    timer_manager.ClearTimer(fuel_broadcast_timer);
}
