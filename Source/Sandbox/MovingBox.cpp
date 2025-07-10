// Fill out your copyright notice in the Description page of Project Settings.

#include "MovingBox.h"

// Sets default values
AMovingBox::AMovingBox() {
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if
    // you don't need it.
    PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AMovingBox::BeginPlay() {
    Super::BeginPlay();
}

// Called every frame
void AMovingBox::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);

    if (move_distance <= 0.0) {
        return;
    }

    auto const fwd{GetActorForwardVector()};
    auto const delta_distance{move_speed * DeltaTime};
    auto const new_location{GetActorLocation() + (fwd * delta_distance)};
    SetActorLocation(new_location);

    this->move_distance -= delta_distance;
}
