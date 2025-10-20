// Fill out your copyright notice in the Description page of Project Settings.

#include "RotatingOnClickActor.h"

// Sets default values
ARotatingOnClickActor::ARotatingOnClickActor() {
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if
    // you don't need it.
    PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ARotatingOnClickActor::BeginPlay() {
    Super::BeginPlay();
}

// Called every frame
void ARotatingOnClickActor::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);

    static constexpr float rotation_duration{2.0};
    static constexpr float rotation_speed{90.0f};

    if (is_rotating) {
        auto const delta_rotation{FRotator(0.0f, rotation_speed * DeltaTime, 0.0f)};
        AddActorLocalRotation(delta_rotation);

        rotation_timer += DeltaTime;
        if (rotation_timer >= rotation_duration) {
            is_rotating = false;
        }
    }
}

void ARotatingOnClickActor::OnClicked() {
    is_rotating = true;
    rotation_timer = 0;
}
