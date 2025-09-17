#include "MovingBox.h"

AMovingBox::AMovingBox() {
    PrimaryActorTick.bCanEverTick = true;
}

void AMovingBox::BeginPlay() {
    Super::BeginPlay();
}

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
