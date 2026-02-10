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
}

void AShipItemCapture::BeginPlay() {
    Super::BeginPlay();
    mesh->bVisibleInSceneCaptureOnly = true;
    mesh->MarkRenderStateDirty();
}
