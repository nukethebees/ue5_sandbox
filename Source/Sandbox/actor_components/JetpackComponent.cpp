#include "Sandbox/actor_components/JetpackComponent.h"

#include "GameFramework/Character.h"

UJetpackComponent::UJetpackComponent() {
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    jetpack_fuel = jetpack_fuel_max;
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
    if (jetpack_fuel <= 0.0f) {
        return;
    }

    is_jetpacking = true;
    SetComponentTickEnabled(true);

    GetWorld()->GetTimerManager().ClearTimer(fuel_recharge_timer);
}
void UJetpackComponent::stop_jetpack() {
    is_jetpacking = false;

    SetComponentTickEnabled(false);

    if (jetpack_fuel < jetpack_fuel_max) {
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
    if (jetpack_fuel <= 0.0f) {
        stop_jetpack();
        return;
    }

    if (auto* character{Cast<ACharacter>(GetOwner())}) {
        FVector const force{0.0f, 0.0f, jetpack_force * delta_time};
        character->LaunchCharacter(force, false, true);
    }

    jetpack_fuel -= jetpack_fuel_consumption_rate * delta_time;
    jetpack_fuel = FMath::Clamp(jetpack_fuel, 0.0f, jetpack_fuel_max);
}
void UJetpackComponent::recharge_fuel() {
    jetpack_fuel += jetpack_fuel_recharge_rate;
    jetpack_fuel = FMath::Clamp(jetpack_fuel, 0.0f, jetpack_fuel_max);

    if (jetpack_fuel >= jetpack_fuel_max) {
        GetWorld()->GetTimerManager().ClearTimer(fuel_recharge_timer);
    }
}
