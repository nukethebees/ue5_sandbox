#include "Sandbox/actors/LoopingLift.h"

ALoopingLift::ALoopingLift() {
    PrimaryActorTick.bCanEverTick = true;
}
void ALoopingLift::BeginPlay() {
    Super::BeginPlay();
    origin = GetActorLocation();
    going_up = true;
}
void ALoopingLift::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);

    auto const up{GetActorLocation()};
    auto const dist_from_origin{up.Z - origin.Z};

    if (going_up) {
        if (dist_from_origin < distance) {
            AddActorWorldOffset(FVector::UpVector * speed * DeltaTime);
        } else {
            going_up = false;
        }
    } else {
        if (dist_from_origin > 0) {
            AddActorWorldOffset(FVector::DownVector * speed * DeltaTime);
        } else {
            going_up = true;
        }
    }
}
