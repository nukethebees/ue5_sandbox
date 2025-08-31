// Fill out your copyright notice in the Description page of Project Settings.

#include "Sandbox/actor_components/SpeedBoostItemComponent.h"

#include "Sandbox/actor_components/SpeedBoostComponent.h"
#include "Sandbox/characters/MyCharacter.h"

USpeedBoostItemComponent::USpeedBoostItemComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}
void USpeedBoostItemComponent::trigger_effect(AActor* target_actor) {
    if (!target_actor) {
        return;
    }

    auto* const boost_component{target_actor->FindComponentByClass<USpeedBoostComponent>()};
    if (!boost_component) {
        return;
    }

    boost_component->apply_speed_boost(speed_boost);
}
