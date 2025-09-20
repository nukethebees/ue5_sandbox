#include "Sandbox/actors/LandMine.h"

#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sandbox/subsystems/CollisionEffectSubsystem.h"

ALandMine::ALandMine() {
    PrimaryActorTick.bCanEverTick = false;

    // Create warning collision component as root (outer detection zone)
    warning_collision_component =
        CreateDefaultSubobject<UCapsuleComponent>(TEXT("WarningCollisionComponent"));
    RootComponent = warning_collision_component;
    warning_collision_component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    warning_collision_component->SetCollisionResponseToAllChannels(ECR_Ignore);
    warning_collision_component->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    warning_collision_component->SetCapsuleSize(75.0f, 15.0f); // Larger warning radius

    // Create inner collision component for detonation
    trigger_collision_component =
        CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionComponent"));
    trigger_collision_component->SetupAttachment(RootComponent);
    trigger_collision_component->SetCollisionEnabled(
        ECollisionEnabled::NoCollision); // Disabled initially
    trigger_collision_component->SetCollisionResponseToAllChannels(ECR_Ignore);
    trigger_collision_component->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    trigger_collision_component->SetCapsuleSize(25.0f, 10.0f); // Smaller detonation trigger

    // Create mesh component
    mesh_component = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    mesh_component->SetupAttachment(RootComponent);
    mesh_component->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Create light component
    light_component = CreateDefaultSubobject<UPointLightComponent>(TEXT("LightComponent"));
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
    explosion_radius_debug->SetSphereRadius(payload_config.explosion_radius);
    explosion_radius_debug->ShapeColor = FColor::Orange;
    explosion_radius_debug->bDrawOnlyIfSelected = true;
#endif

    update_debug_sphere();
}

void ALandMine::BeginPlay() {
    Super::BeginPlay();

    // Bind warning collision events
    if (warning_collision_component) {
        warning_collision_component->OnComponentBeginOverlap.AddDynamic(
            this, &ALandMine::on_warning_enter);
        warning_collision_component->OnComponentEndOverlap.AddDynamic(this,
                                                                      &ALandMine::on_warning_exit);
    }

    // Set runtime location in payload config
    payload_config.mine_location = GetActorLocation();

    try_emplace_subsystem_payload<UCollisionEffectSubsystem, FLandMinePayload>(*this,
                                                                               payload_config);
}

void ALandMine::on_pre_collision_effect(AActor& other_actor) {
    log_verbose(TEXT("Landmine triggered by actor: %s"), *other_actor.GetName());

    // Change state to detonating
    change_state(ELandMineState::Detonating);

    // Spawn explosion particle effect if configured
    if (explosion_effect) {
        if (auto* world{GetWorld()}) {
            FVector const location{GetActorLocation()};
            FRotator const rotation{GetActorRotation()};
            UGameplayStatics::SpawnEmitterAtLocation(world, explosion_effect, location, rotation);
        }
    }
}

void ALandMine::change_state(ELandMineState new_state) {
    if (current_state == new_state) {
        return;
    }

    current_state = new_state;

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
    }

    // Manage collision component states
    if (trigger_collision_component && warning_collision_component) {
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
}

void ALandMine::on_warning_enter(UPrimitiveComponent* OverlappedComponent,
                                 AActor* OtherActor,
                                 UPrimitiveComponent* OtherComp,
                                 int32 OtherBodyIndex,
                                 bool bFromSweep,
                                 FHitResult const& SweepResult) {
    if (current_state == ELandMineState::Active && OtherActor) {
        log_verbose(TEXT("Warning zone entered by actor: %s"), *OtherActor->GetName());
        change_state(ELandMineState::Warning);
    }
}

void ALandMine::on_warning_exit(UPrimitiveComponent* OverlappedComponent,
                                AActor* OtherActor,
                                UPrimitiveComponent* OtherComp,
                                int32 OtherBodyIndex) {
    if (current_state == ELandMineState::Warning && OtherActor) {
        log_verbose(TEXT("Warning zone exited by actor: %s"), *OtherActor->GetName());
        change_state(ELandMineState::Active);
    }
}

void ALandMine::update_debug_sphere() {
#if WITH_EDITORONLY_DATA
    if (explosion_radius_debug) {
        explosion_radius_debug->SetSphereRadius(payload_config.explosion_radius /
                                                explosion_radius_debug->GetShapeScale());
    }
#endif
}

#if WITH_EDITOR
void ALandMine::OnConstruction(const FTransform& Transform) {
    Super::OnConstruction(Transform);
    update_debug_sphere();
}
#endif
