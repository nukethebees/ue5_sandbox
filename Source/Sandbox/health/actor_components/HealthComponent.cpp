// Fill out your copyright notice in the Description page of Project Settings.

#include "Sandbox/health/actor_components/HealthComponent.h"

UHealthComponent::UHealthComponent()
    : initial_health(max_health) {
    PrimaryComponentTick.bCanEverTick = false;
}
void UHealthComponent::BeginPlay() {
    Super::BeginPlay();

    set_health(initial_health < 0.0f ? max_health : initial_health);

    check(Cast<IDeathHandler>(GetOwner()));
}
