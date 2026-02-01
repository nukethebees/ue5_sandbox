#include "Sandbox/players/playable/space_ship/SpaceShip.h"

#include "Sandbox/logging/SandboxLogCategories.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"

ASpaceShip::ASpaceShip()
    : camera(CreateDefaultSubobject<UCameraComponent>(TEXT("camera")))
    , spring_arm(CreateDefaultSubobject<USpringArmComponent>(TEXT("spring_arm")))
    , ship_mesh(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ship_mesh")))
    , collision_box(CreateDefaultSubobject<UBoxComponent>(TEXT("collision_box"))) {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    camera->SetupAttachment(spring_arm);
    spring_arm->SetupAttachment(RootComponent);
    ship_mesh->SetupAttachment(RootComponent);
    collision_box->SetupAttachment(RootComponent);
}
void ASpaceShip::Tick(float dt) {
    Super::Tick(dt);

    FRotator const delta_rotation(
        rotation_input.Y * rotation_speed * dt, rotation_input.X * rotation_speed * dt, 0.f);
    AddActorLocalRotation(delta_rotation);

    auto const delta_pos{velocity * dt};
    SetActorLocation(GetActorLocation() + delta_pos, true);
}

void ASpaceShip::BeginPlay() {
    Super::BeginPlay();

    velocity = GetActorForwardVector() * cruise_speed;
}

void ASpaceShip::turn(FVector2D direction) {
    UE_LOG(LogSandboxActor, Verbose, TEXT("Turning: %s"), *direction.ToString());

    rotation_input = direction;
}
