#include "TestTarget.h"

#include <Components/SceneComponent.h>
#include <Components/StaticMeshComponent.h>

ATestTarget::ATestTarget()
    : mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    mesh->SetupAttachment(RootComponent);
}
