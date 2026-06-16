#include "TestStaticTurretsProxy.h"

#include "Sandbox/logging/SandboxLogCategories.h"
#include "TestStaticTurrets.h"
#include "TestStaticTurretsConfig.h"

#include <SandboxCore/actor_utils.h>

#include <Components/ArrowComponent.h>
#include <Components/CapsuleComponent.h>
#include <Components/SceneComponent.h>
#include <Components/SphereComponent.h>
#include <Components/StaticMeshComponent.h>
#include <EngineUtils.h>

ATestStaticTurretsProxy::ATestStaticTurretsProxy()
    : mesh{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("mesh"))}
    , collision{CreateDefaultSubobject<UCapsuleComponent>(TEXT("collision"))}
    , detection{CreateDefaultSubobject<USphereComponent>(TEXT("detection"))}
    , fire_point{CreateDefaultSubobject<UArrowComponent>(TEXT("fire_point"))} {
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("root"));

    mesh->SetupAttachment(RootComponent);
    collision->SetupAttachment(RootComponent);
    detection->SetupAttachment(RootComponent);
    fire_point->SetupAttachment(RootComponent);

    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;

    configure_component(*mesh);
    configure_component(*collision);
    configure_component(*detection);
}

void ATestStaticTurretsProxy::configure_component(UPrimitiveComponent& component) {
    component.SetCollisionEnabled(ECollisionEnabled::NoCollision);
    component.SetGenerateOverlapEvents(false);
    component.SetCanEverAffectNavigation(false);
    component.SetCastShadow(false);
    component.SetAffectDistanceFieldLighting(false);
}

#if WITH_EDITOR
void ATestStaticTurretsProxy::save_configuration_to_asset() {
    if (!actor_config) { return; }

    actor_config->Modify();

    actor_config->detection_radius = detection->GetUnscaledSphereRadius();
    actor_config->fire_point_offset = fire_point->GetRelativeTransform();

    actor_config->MarkPackageDirty();
}

void ATestStaticTurretsProxy::apply_asset_configuration() {
    if (!actor_config) {
        UE_LOG(
            LogSandboxLearning,
            Warning,
            TEXT("ATestStaticTurretsProxy::apply_asset_configuration: actor_config is nullptr."));
        return;
    }

    mesh->SetStaticMesh(actor_config->mesh);
    detection->SetSphereRadius(actor_config->detection_radius);
    fire_point->SetRelativeTransform(actor_config->fire_point_offset);

    detection->SetVisibility(actor_config->show_collision);
}
void ATestStaticTurretsProxy::apply_asset_configuration_to_all_instances() {
    ml::for_each_instance(*this, [](ThisClass& x) { x.apply_asset_configuration(); });
}

#endif
