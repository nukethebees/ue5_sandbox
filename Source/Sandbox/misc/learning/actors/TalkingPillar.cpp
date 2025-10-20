// Fill out your copyright notice in the Description page of Project Settings.

#include "TalkingPillar.h"

// Sets default values
ATalkingPillar::ATalkingPillar() {
    // Set this actor to call Tick() every frame.  You can turn this off to improve performance if
    // you don't need it.
    PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ATalkingPillar::BeginPlay() {
    Super::BeginPlay();
}

// Called every frame
void ATalkingPillar::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);
}
