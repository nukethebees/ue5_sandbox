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

    is_jetpacking = true;
    SetComponentTickEnabled(true);

    GetWorld()->GetTimerManager().ClearTimer(fuel_recharge_timer);
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
        FVector const force{0.0f, 0.0f, lift_force * delta_time};
        character->LaunchCharacter(force, false, true);
    }

    fuel -= fuel_consumption_rate * delta_time;
    fuel = FMath::Clamp(fuel, 0.0f, fuel_max);
}
void UJetpackComponent::recharge_fuel() {
    fuel += fuel_recharge_rate;
    fuel = FMath::Clamp(fuel, 0.0f, fuel_max);

    if (fuel >= fuel_max) {
        GetWorld()->GetTimerManager().ClearTimer(fuel_recharge_timer);
    }
}
