#include "Sandbox/ui/ship_hud/ShipItemCapture.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

AShipItemCapture::AShipItemCapture()
    : mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh"))}
    , camera{CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("camera"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    mesh->SetupAttachment(RootComponent);
    camera->SetupAttachment(RootComponent);
    camera->CaptureSource = ESceneCaptureSource::SCS_SceneColorSceneDepth;
    camera->ProjectionType = ECameraProjectionMode::Orthographic;

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
}

void AShipItemCapture::BeginPlay() {
    Super::BeginPlay();
    mesh->bVisibleInSceneCaptureOnly = true;
    mesh->MarkRenderStateDirty();

    SetActorTickEnabled(!rotation_speed.IsNearlyZero());
}

void AShipItemCapture::Tick(float dt) {
    Super::Tick(dt);

    auto const delta_rotation{rotation_speed * dt};
    mesh->AddLocalRotation(delta_rotation);
}
