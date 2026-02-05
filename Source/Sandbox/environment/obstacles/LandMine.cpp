#include "Sandbox/environment/obstacles/LandMine.h"

#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"

#include "Sandbox/constants/collision_channels.h"
#include "Sandbox/environment/utilities/actor_utils.h"
#include "Sandbox/health/HealthComponent.h"
#include "Sandbox/interaction/CollisionEffectSubsystem.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

ALandMine::ALandMine()
    : warning_collision_component{CreateDefaultSubobject<UCapsuleComponent>(
          TEXT("WarningCollisionComponent"))}
    , trigger_collision_component{CreateDefaultSubobject<UCapsuleComponent>(
          TEXT("CollisionComponent"))}
    , mesh_component{CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"))}
    , light_component{CreateDefaultSubobject<UPointLightComponent>(TEXT("LightComponent"))}
    , health_component{CreateDefaultSubobject<UHealthComponent>(TEXT("Health"))} {
    PrimaryActorTick.bCanEverTick = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    // Create warning collision component as root (outer detection zone)
    warning_collision_component->SetupAttachment(RootComponent);
    warning_collision_component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    warning_collision_component->SetCollisionResponseToAllChannels(ECR_Ignore);
    warning_collision_component->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    // Create inner collision component for detonation
    trigger_collision_component->SetupAttachment(RootComponent);
    trigger_collision_component->SetCollisionEnabled(
        ECollisionEnabled::NoCollision); // Disabled initially
    trigger_collision_component->SetCollisionResponseToAllChannels(ECR_Ignore);
    trigger_collision_component->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    update_trigger_sizes();

    // Create mesh component
    mesh_component->SetupAttachment(RootComponent);
    mesh_component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    // Allow destruction from bullets
    mesh_component->SetCollisionResponseToChannel(ml::collision::projectile, ECR_Block);

    // Create light component
    light_component->SetupAttachment(RootComponent);
    light_component->SetLightColor(colours.active);
    light_component->SetIntensity(100.0f);
    light_component->SetAttenuationRadius(200.0f);

#if WITH_EDITORONLY_DATA
    // Create debug sphere to visualize explosion radius (editor only)
    explosion_radius_debug = CreateDefaultSubobject<USphereComponent>(TEXT("ExplosionRadiusDebug"));
    explosion_radius_debug->SetupAttachment(RootComponent);
    explosion_radius_debug->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    explosion_radius_debug->SetCanEverAffectNavigation(false);
    explosion_radius_debug->SetHiddenInGame(true);
    explosion_radius_debug->SetSphereRadius(payload_config.explosion_config.explosion_radius);
    explosion_radius_debug->ShapeColor = FColor::Orange;
    explosion_radius_debug->bDrawOnlyIfSelected = true;
#endif

    constexpr float max_health{1.0f};
    health_component->max_health = max_health;
    health_component->initial_health = max_health;

    update_debug_sphere();
}

UPrimitiveComponent* ALandMine::get_collision_component() {
    return trigger_collision_component;
}
float ALandMine::get_destruction_delay() const {
    return payload_config.detonation_delay;
}
void ALandMine::on_pre_collision_effect(AActor& other_actor) {
    constexpr auto logger{NestedLogger<"on_pre_collision_effect">()};
    logger.log_verbose(TEXT("Landmine triggered by actor: %s"), *other_actor.GetName());
    change_state(ELandMineState::Detonating);
}

void ALandMine::OnConstruction(FTransform const& Transform) {
    Super::OnConstruction(Transform);
#if WITH_EDITOR
    update_debug_sphere();
#endif
    update_trigger_sizes();
}
void ALandMine::BeginPlay() {
    constexpr auto logger{NestedLogger<"BeginPlay">()};
    Super::BeginPlay();

    change_state(current_state);

    // Bind warning collision events
    check(warning_collision_component);
    if (warning_collision_component) {
        warning_collision_component->OnComponentBeginOverlap.AddDynamic(
            this, &ALandMine::on_warning_enter);
        warning_collision_component->OnComponentEndOverlap.AddDynamic(this,
                                                                      &ALandMine::on_warning_exit);
    } else {
        logger.log_warning(TEXT("warning_collision_component is nullptr"));
    }

    // Set runtime location in payload config
    payload_config.mine_location = GetActorLocation();

    try_add_subsystem_payload<UCollisionEffectSubsystem>(*this, payload_config);
}

void ALandMine::change_state(ELandMineState new_state) {
    constexpr auto logger{NestedLogger<"change_state">()};

    current_state = new_state;

#if !UE_BUILD_SHIPPING
    logger.log_verbose(TEXT("Setting mine %s to %s state."),
                       *ml::get_best_display_name(*this),
                       *UEnum::GetValueAsString(current_state));
#endif

    // Update light color based on state
    FLinearColor target_color{FLinearColor::White};
    switch (current_state) {
        case ELandMineState::Active: {
            target_color = colours.active;
            break;
        }
        case ELandMineState::Warning: {
            target_color = colours.warning;
            break;
        }
        case ELandMineState::Detonating: {
            target_color = colours.detonating;
            break;
        }
        case ELandMineState::Deactivated: {
            target_color = colours.deactivated;
            break;
        }
    }

    if (light_component) {
        light_component->SetLightColor(target_color);
    } else {
        logger.log_warning(TEXT("light_component is nullptr"));
    }

    RETURN_IF_NULLPTR(trigger_collision_component);
    RETURN_IF_NULLPTR(warning_collision_component);

    // Manage collision component states
    switch (current_state) {
        case ELandMineState::Active: {
            warning_collision_component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            trigger_collision_component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            break;
        }
        case ELandMineState::Warning: {
            warning_collision_component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            trigger_collision_component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            break;
        }
        case ELandMineState::Detonating: {
            // Keep collisions active during detonation
            break;
        }
        case ELandMineState::Deactivated: {
            warning_collision_component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            trigger_collision_component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            break;
        }
    }
}

void ALandMine::on_warning_enter(UPrimitiveComponent* overlapped_component,
                                 AActor* other_actor,
                                 UPrimitiveComponent* OtherComp,
                                 int32 other_body_index,
                                 bool from_sweep,
                                 FHitResult const& sweep_result) {
    constexpr auto logger{NestedLogger<"change_state">()};
    RETURN_IF_NULLPTR(other_actor);

    if (current_state == ELandMineState::Active) {
        logger.log_verbose(TEXT("Warning zone entered by actor: %s"), *other_actor->GetName());
        change_state(ELandMineState::Warning);
    } else {
        logger.log_verbose(TEXT("Not in active state."));
    }
}

void ALandMine::on_warning_exit(UPrimitiveComponent* overlapped_component,
                                AActor* other_actor,
                                UPrimitiveComponent* OtherComp,
                                int32 other_body_index) {
    if (current_state == ELandMineState::Warning && other_actor) {
        log_verbose(TEXT("Warning zone exited by actor: %s"), *other_actor->GetName());
        change_state(ELandMineState::Active);
    }
}

void ALandMine::handle_death() {
    TRY_INIT_PTR(world, GetWorld());
    payload_config.detonation_delay = 0.0f;
    payload_config.explode(*world);
    change_state(ELandMineState::Detonating);
    Destroy();
}

void ALandMine::update_debug_sphere() {
#if WITH_EDITORONLY_DATA
    if (explosion_radius_debug) {
        explosion_radius_debug->SetSphereRadius(payload_config.explosion_config.explosion_radius /
                                                explosion_radius_debug->GetShapeScale());
    }
#endif
}
void ALandMine::update_trigger_sizes() {
    warning_collision_component->SetCapsuleSize(warning_radius, collision_half_height);
    trigger_collision_component->SetCapsuleSize(trigger_radius, collision_half_height);
}
