#include "SpaceGame/defences/turrets/TestStaticTurretsProxy.h"

#include "SpaceGame/entities/TestProxyActorFunctions.h"
#include "SpaceGamePresentation/entities/TestTeamVisualData.h"
#include "SpaceGameSimulation/support/logging/SandboxLogCategories.h"

#include <SandboxCoreEngine/actor_utils.h>

#include <Components/ArrowComponent.h>
#include <Components/CapsuleComponent.h>
#include <Components/SceneComponent.h>
#include <Components/SphereComponent.h>
#include <Components/StaticMeshComponent.h>
#include <EngineUtils.h>
#if WITH_EDITOR
#include <ScopedTransaction.h>
#endif

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

void ATestStaticTurretsProxy::OnConstruction(FTransform const& transform) {
    Super::OnConstruction(transform);
    ml::set_proxy_actor_name(*this, TEXT("StaticTurret"), team);
    apply_actor_configuration();
}

void ATestStaticTurretsProxy::configure_component(UPrimitiveComponent& component) {
    component.SetCollisionEnabled(ECollisionEnabled::NoCollision);
    component.SetGenerateOverlapEvents(false);
    component.SetCanEverAffectNavigation(false);
    component.SetCastShadow(false);
    component.SetAffectDistanceFieldLighting(false);
}

void ATestStaticTurretsProxy::apply_actor_configuration() {
    if (!actor_config) {
        return;
    }

    mesh->SetStaticMesh(actor_config->mesh);
    detection->SetSphereRadius(actor_config->detection_radius);
    fire_point->SetRelativeTransform(actor_config->fire_point_offset);
    detection->SetVisibility(actor_config->show_collision);

    if (IsValid(actor_config->team_visual_data) && ml::is_valid(team)) {
        actor_config->team_visual_data->ensure_all_team_colours_exist();
        auto const colour_cache{
            UTestTeamVisualData::build_team_colour_cache(actor_config->team_visual_data)};
        mesh->SetCustomPrimitiveDataVector3f(0, FVector3f{colour_cache[team]});
    }
}

#if WITH_EDITOR
void ATestStaticTurretsProxy::apply_asset_configuration() {
    auto const* const config{ml::resolve_proxy_level_config(*this)};
    if (!IsValid(config)) {
        UE_LOG(LogSandboxLearning,
               Warning,
               TEXT("Cannot apply turret configuration: requires exactly one orchestrator with a "
                    "level config."));
        return;
    }
    FScopedTransaction const transaction{
        NSLOCTEXT("TurretProxy", "Apply", "Apply turret configuration")};
    Modify();
    actor_config = &config->turrets;
    mesh->Modify();
    detection->Modify();
    fire_point->Modify();
    apply_actor_configuration();
    MarkPackageDirty();
}
void ATestStaticTurretsProxy::apply_asset_configuration_to_all_instances() {
    FScopedTransaction const transaction{
        NSLOCTEXT("TurretProxy", "ApplyAll", "Apply configuration to all turrets")};
    ml::for_each_instance(*this, [](ThisClass& x) { x.apply_asset_configuration(); });
}

#endif
