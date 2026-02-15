#include "Sandbox/environment/SandboxSkybox.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

ASandboxSkybox::ASandboxSkybox()
    : mesh(CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh"))) {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    mesh->SetupAttachment(RootComponent);
    mesh->bTreatAsBackgroundForOcclusion = true;
    mesh->bUseAsOccluder = true;

    mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    mesh->SetCanEverAffectNavigation(false);

    mesh->SetCastShadow(false);
    mesh->SetAffectDistanceFieldLighting(false);
    mesh->SetAffectDynamicIndirectLighting(false);
    mesh->SetAffectIndirectLightingWhileHidden(false);
}
