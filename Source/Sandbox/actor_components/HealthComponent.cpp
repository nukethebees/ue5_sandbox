// Fill out your copyright notice in the Description page of Project Settings.

#include "Sandbox/actor_components/HealthComponent.h"

UHealthComponent::UHealthComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}
void UHealthComponent::BeginPlay() {
    Super::BeginPlay();

    set_health(initial_health == NO_INITIAL_HEALTH ? max_health : initial_health);
}
