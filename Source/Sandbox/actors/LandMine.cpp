#include "Sandbox/actors/LandMine.h"

#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sandbox/subsystems/CollisionEffectSubsystem.h"

ALandMine::ALandMine() {
    PrimaryActorTick.bCanEverTick = false;

    // Create collision component as root
    collision_component = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionComponent"));
    RootComponent = collision_component;
    collision_component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    collision_component->SetCollisionResponseToAllChannels(ECR_Ignore);
    collision_component->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    collision_component->SetBoxExtent(FVector{25.0f, 25.0f, 10.0f}); // Small trigger box

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
    explosion_radius_debug->SetSphereRadius(explosion_radius);
    explosion_radius_debug->ShapeColor = FColor::Orange;
    explosion_radius_debug->bDrawOnlyIfSelected = true;
#endif

    update_debug_sphere();
}

void ALandMine::BeginPlay() {
    Super::BeginPlay();

    // Set light to active color
    light_component->SetLightColor(colours.active);

    try_emplace_subsystem_payload<UCollisionEffectSubsystem, FLandMinePayload>(
        *this, max_damage, explosion_radius, explosion_force, GetActorLocation());
}

void ALandMine::on_pre_collision_effect(AActor& other_actor) {
    log_verbose(TEXT("Landmine triggered by actor: %s"), *other_actor.GetName());

    // Change light to disabled color to indicate detonation
    light_component->SetLightColor(colours.disabled);

    // Spawn explosion particle effect if configured
    if (explosion_effect) {
        if (auto* world{GetWorld()}) {
            FVector const location{GetActorLocation()};
            FRotator const rotation{GetActorRotation()};
            UGameplayStatics::SpawnEmitterAtLocation(world, explosion_effect, location, rotation);
        }
    }
}

void ALandMine::update_debug_sphere() {
#if WITH_EDITORONLY_DATA
    if (explosion_radius_debug) {
        explosion_radius_debug->SetSphereRadius(explosion_radius /
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
