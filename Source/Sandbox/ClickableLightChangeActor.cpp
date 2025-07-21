// Fill out your copyright notice in the Description page of Project Settings.

#include "ClickableLightChangeActor.h"

// Sets default values
AClickableLightChangeActor::AClickableLightChangeActor() {
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if
    // you don't need it.
    PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AClickableLightChangeActor::BeginPlay() {
    Super::BeginPlay();

    point_light = FindComponentByClass<UPointLightComponent>();
    point_light->SetLightColor(colours[current_colour_index]);
}

// Called every frame
void AClickableLightChangeActor::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);
}

void AClickableLightChangeActor::OnClicked() {
    current_colour_index = (current_colour_index + 1) % colours.size();
    point_light->SetLightColor(colours[current_colour_index]);

    return;
}
