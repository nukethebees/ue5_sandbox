#include "SpaceGame/ships/fighter/TestFighterProxy.h"

#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGamePresentation/entities/TestTeamVisualData.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <Components/InstancedStaticMeshComponent.h>
#include <Engine/StaticMesh.h>
#include <Engine/World.h>

namespace ioj {
ATestFighterProxy::ATestFighterProxy()
    : mesh{CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("FighterMesh"))} {
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
    refresh_preview();
}

#if WITH_EDITOR
void ATestFighterProxy::PostRegisterAllComponents() {
    Super::PostRegisterAllComponents();
    if (HasAnyFlags(RF_WasLoaded) && GetWorld()->WorldType == EWorldType::Editor) {
        refresh_preview();
    }
}
#endif

void ATestFighterProxy::refresh_preview() {
    auto const* config{s7_level_config::load_canonical()};
    if (!IsValid(config) || !IsValid(config->fighters.team_visual_data)) {
        UE_LOG(LogSandbox, Warning, TEXT("Fighter preview requires the shared fighter palette."));
        return;
    }

    mesh->SetStaticMesh(config->fighters.mesh);
    mesh->SetNumCustomDataFloats(3);
    if (mesh->GetInstanceCount() == 0) {
        mesh->AddInstance(FTransform::Identity);
    }

    auto const colour{config->fighters.team_visual_data->get_colour(team)};
    float const rgb[]{colour.R, colour.G, colour.B};
    mesh->SetCustomData(0, rgb, true);
}
}
