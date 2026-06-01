#include "TestStaticTurretsProxy.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "TestStaticTurrets.h"
#include "TestStaticTurretsConfig.h"

#include <Components/CapsuleComponent.h>
#include <Components/SceneComponent.h>
#include <Components/SphereComponent.h>
#include <Components/StaticMeshComponent.h>

ATestStaticTurretsProxy::ATestStaticTurretsProxy()
    : mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh"))}
    , collision{CreateDefaultSubobject<UCapsuleComponent>(TEXT("collision"))}
    , detection{CreateDefaultSubobject<USphereComponent>(TEXT("detection"))}
    , fire_point{CreateDefaultSubobject<USceneComponent>(TEXT("fire_point"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    mesh->SetupAttachment(RootComponent);
    collision->SetupAttachment(RootComponent);
    detection->SetupAttachment(RootComponent);
    fire_point->SetupAttachment(RootComponent);
}

void ATestStaticTurretsProxy::Tick(float dt) {
    Super::Tick(dt);
}
void ATestStaticTurretsProxy::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);

    if (!actor_config) {
        return;
    }

    if (actor_config->mesh != nullptr) {
        mesh->SetStaticMesh(actor_config->mesh);
    }
}
void ATestStaticTurretsProxy::BeginPlay() {
    Super::BeginPlay();

    if (!batch_actor) {
        UE_LOG(
            LogSandboxLearning, Warning, TEXT("ATestStaticTurretsProxy: batch_actor is nullptr."));
        return;
    }

    batch_actor->spawn_instance(GetActorTransform(), team);
    Destroy();
}

#if WITH_EDITOR
void ATestStaticTurretsProxy::save_configuration_to_asset() {
    if (!actor_config) {
        return;
    }

    actor_config->detection_radius = detection->GetUnscaledSphereRadius();
    actor_config->fire_point_offset = fire_point->GetRelativeTransform();
}
#endif
