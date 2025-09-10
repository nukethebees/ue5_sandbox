#include "Sandbox/actor_components/JetpackComponent.h"

#include "GameFramework/Character.h"

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
        constexpr float jetpack_timer_interval{0.2f};
        constexpr bool jetpack_timer_loops{true};
        GetWorld()->GetTimerManager().SetTimer(fuel_recharge_timer,
                                               this,
                                               &UJetpackComponent::recharge_fuel,
                                               jetpack_timer_interval,
                                               jetpack_timer_loops);
    }
}
void UJetpackComponent::apply_jetpack_force(float delta_time) {
    if (fuel <= 0.0f) {
        stop_jetpack();
        return;
    }

    if (auto* character{Cast<ACharacter>(GetOwner())}) {
        auto const fuel_needed{fuel_consumption_rate * delta_time};
        auto const fuel_ratio{FMath::Clamp(fuel / fuel_needed, 0.0f, 1.0f)};
        auto const scaled_force{lift_force * delta_time * fuel_ratio};

        FVector const force{0.0f, 0.0f, scaled_force};
        character->LaunchCharacter(force, false, true);
    }

    fuel -= fuel_consumption_rate * delta_time;
    fuel = FMath::Clamp(fuel, 0.0f, fuel_max);
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
