#include "MovesWhenClickedActor.h"

AMovesWhenClickedActor::AMovesWhenClickedActor() {
    PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AMovesWhenClickedActor::BeginPlay() {
    Super::BeginPlay();
}

// Called every frame
void AMovesWhenClickedActor::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);

    if (is_moving) {
        auto fwd{GetActorForwardVector()};
        AddActorWorldOffset(fwd * move_speed * DeltaTime);

        move_timer += DeltaTime;
        if (move_timer >= move_duration) {
            is_moving = false;
        }
    }
}

void AMovesWhenClickedActor::OnClicked() {
    is_moving = true;
    move_timer = 0;
}
