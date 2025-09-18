#include "Sandbox/actor_components/SpeedBoostItemComponent.h"

#include "Sandbox/actor_components/SpeedBoostComponent.h"
#include "Sandbox/utilities/CollisionEffectHelpers.h"

USpeedBoostItemComponent::USpeedBoostItemComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void USpeedBoostItemComponent::BeginPlay() {
    Super::BeginPlay();
    CollisionEffectHelpers::register_with_collision_system(this);
}

void USpeedBoostItemComponent::execute_effect(AActor* other_actor) {
    if (!other_actor) {
        return;
    }

    if (auto* boost_component{other_actor->FindComponentByClass<USpeedBoostComponent>()}) {
        boost_component->apply_speed_boost(speed_boost);
    }
}
