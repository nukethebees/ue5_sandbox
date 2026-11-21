#include "SpaceGame/ships/fighter/TestFighterProxy.h"

#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <Components/StaticMeshComponent.h>
#include <Engine/StaticMesh.h>

namespace ioj {
ATestFighterProxy::ATestFighterProxy()
    : mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FighterMesh"))} {
    SetRootComponent(mesh);
    mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    mesh->SetGenerateOverlapEvents(false);
    mesh->SetCanEverAffectNavigation(false);
    mesh->SetHiddenInGame(true);
#if WITH_EDITORONLY_DATA
    bIsEditorOnlyActor = true;
#endif
}

void ATestFighterProxy::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
    auto const* config{s7_level_config::load_canonical()};
    if (IsValid(config)) {
        mesh->SetStaticMesh(config->fighters.mesh);
    } else {
        UE_LOG(LogSandbox, Warning, TEXT("Fighter preview requires the level configuration."));
    }
}
}
