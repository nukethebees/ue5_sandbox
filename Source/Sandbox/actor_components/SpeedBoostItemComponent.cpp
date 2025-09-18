#include "Sandbox/actor_components/SpeedBoostItemComponent.h"

#include "Sandbox/actor_components/SpeedBoostComponent.h"

USpeedBoostItemComponent::USpeedBoostItemComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void USpeedBoostItemComponent::execute_pickup_effect(AActor* target_actor) {
    if (!target_actor) {
        return;
    }

    if (auto* boost_component{target_actor->FindComponentByClass<USpeedBoostComponent>()}) {
        boost_component->apply_speed_boost(speed_boost);
    }
}
