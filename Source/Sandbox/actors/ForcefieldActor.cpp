#include "ForcefieldActor.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "Components/PrimitiveComponent.h"

AForcefieldActor::AForcefieldActor() {
    PrimaryActorTick.bCanEverTick = false;

    FVector const box_extent{FVector{100.0f, 100.0f, 100.0f}};

    constexpr auto box_mobility{EComponentMobility::Static};

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
    RootComponent->SetMobility(box_mobility);

    collision_volume = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionVolume"));
    collision_volume->SetupAttachment(RootComponent);
    collision_volume->SetBoxExtent(box_extent);
    collision_volume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    collision_volume->SetCollisionProfileName(TEXT("BlockAll"));
    collision_volume->SetMobility(box_mobility);

    // Create barrier mesh
    barrier_mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrierMesh"));
    barrier_mesh->SetupAttachment(RootComponent);
    barrier_mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    barrier_mesh->SetVisibility(true);
    barrier_mesh->SetMobility(box_mobility);

    // Create particle system
    sparkle_particles = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("SparkleParticles"));
    sparkle_particles->SetupAttachment(RootComponent);
    sparkle_particles->SetAutoActivate(false);

    // Create distortion effect
    distortion_effect = CreateDefaultSubobject<UPostProcessComponent>(TEXT("DistortionEffect"));
    distortion_effect->SetupAttachment(RootComponent);
    distortion_effect->bEnabled = false;
}

void AForcefieldActor::OnConstruction(FTransform const& Transform) {
    Super::OnConstruction(Transform);

    // Resize barrier mesh to match collision volume
    if (barrier_mesh && barrier_mesh->GetStaticMesh()) {
        // Get the size of the collision volume (extent is half-size)
        FVector const collision_size{collision_volume->GetScaledBoxExtent() * 2.0f};
        FVector const mesh_size{barrier_mesh->GetStaticMesh()->GetBoundingBox().GetSize()};

        // Calculate the required scale and set it, avoiding division by zero
        if (!mesh_size.IsNearlyZero()) {
            FVector const desired_scale{collision_size / mesh_size};
            barrier_mesh->SetWorldScale3D(desired_scale);
        }
    }
}

void AForcefieldActor::BeginPlay() {
    Super::BeginPlay();

    // Create dynamic material instance for the barrier
    if (barrier_mesh->GetMaterial(0)) {
        barrier_material_instance =
            UMaterialInstanceDynamic::Create(barrier_mesh->GetMaterial(0), this);
        if (barrier_material_instance) {
            barrier_mesh->SetMaterial(0, barrier_material_instance);
        }
    }

    // Initialize in inactive state
    current_state = EForcefieldState::Active;
    update_visual_effects();
    update_collision_blocking();

    // Start debug timer if needed
    if (show_debug_visualization) {
        GetWorldTimerManager().SetTimer(debug_timer_handle,
                                        this,
                                        &AForcefieldActor::draw_debug_info,
                                        debug_update_interval,
                                        true);
    }

    log_verbose(TEXT("Forcefield initialized at %s"), *GetActorLocation().ToString());
}

void AForcefieldActor::trigger_activation(AActor* instigator) {
    if (!can_activate(instigator)) {
        log_verbose(TEXT("Forcefield cannot activate."));
        return;
    }

    switch (current_state) {
        case EForcefieldState::Inactive: {
            log_verbose(TEXT("Starting forcefield activation"));
            start_activation();
            break;
        }
        case EForcefieldState::Active: {
            log_verbose(TEXT("Starting forcefield deactivation"));
            start_deactivation();
            break;
        }
        default: {
            log_verbose(TEXT("Ignoring activation - currently in state: %d"),
                        static_cast<int32>(current_state));
            break;
        }
    }
}

bool AForcefieldActor::can_activate(AActor const* instigator) const {
    // Can only be activated when inactive or active (for deactivation)
    return current_state == EForcefieldState::Inactive || current_state == EForcefieldState::Active;
}

void AForcefieldActor::start_activation() {
    current_state = EForcefieldState::Activating;

    // Clear any existing timers
    GetWorldTimerManager().ClearTimer(state_timer_handle);
    GetWorldTimerManager().ClearTimer(cooldown_timer_handle);

    // Start visual effects immediately
    update_visual_effects();

    // Set timer to complete activation
    GetWorldTimerManager().SetTimer(state_timer_handle,
                                    this,
                                    &AForcefieldActor::complete_activation,
                                    transition_duration,
                                    false);

    log_verbose(TEXT("Activation started - will complete in %.1f seconds"), transition_duration);
}

void AForcefieldActor::complete_activation() {
    current_state = EForcefieldState::Active;
    update_collision_blocking();

    log_verbose(TEXT("Forcefield activation completed - now blocking"));
}

void AForcefieldActor::start_deactivation() {
    current_state = EForcefieldState::Deactivating;

    // Clear any existing timers
    GetWorldTimerManager().ClearTimer(state_timer_handle);
    GetWorldTimerManager().ClearTimer(cooldown_timer_handle);

    // Stop blocking immediately for safety
    update_collision_blocking();

    // Set timer to complete deactivation
    GetWorldTimerManager().SetTimer(state_timer_handle,
                                    this,
                                    &AForcefieldActor::complete_deactivation,
                                    transition_duration,
                                    false);

    log_verbose(TEXT("Deactivation started - will complete in %.1f seconds"), transition_duration);
}

void AForcefieldActor::complete_deactivation() {
    update_visual_effects();
    start_cooldown();

    log_verbose(TEXT("Forcefield deactivation completed - starting cooldown"));
}

void AForcefieldActor::start_cooldown() {
    current_state = EForcefieldState::Cooldown;

    // Set timer to complete cooldown
    GetWorldTimerManager().SetTimer(cooldown_timer_handle,
                                    this,
                                    &AForcefieldActor::complete_cooldown,
                                    cooldown_duration,
                                    false);

    log_verbose(TEXT("Cooldown started - will complete in %.1f seconds"), cooldown_duration);
}

void AForcefieldActor::complete_cooldown() {
    current_state = EForcefieldState::Inactive;

    log_verbose(TEXT("Cooldown completed - forcefield ready for activation"));
}

void AForcefieldActor::update_visual_effects() {
    bool const should_be_visible{current_state == EForcefieldState::Activating ||
                                 current_state == EForcefieldState::Active ||
                                 current_state == EForcefieldState::Deactivating};

    // Update barrier mesh visibility
    barrier_mesh->SetVisibility(should_be_visible);

    // Update particle effects
    if (should_be_visible && !sparkle_particles->IsActive()) {
        sparkle_particles->Activate();
    } else if (!should_be_visible && sparkle_particles->IsActive()) {
        sparkle_particles->Deactivate();
    }

    // Update distortion effect
    if (distortion_effect && enable_distortion_effect) {
        distortion_effect->bEnabled = should_be_visible;
        if (distortion_effect->Settings.WeightedBlendables.Array.Num() > 0) {
            distortion_effect->Settings.WeightedBlendables.Array[0].Weight =
                should_be_visible ? distortion_strength : 0.0f;
        }
    }

    // Update material parameters
    if (barrier_material_instance) {
        float const opacity{should_be_visible ? 0.3f : 0.0f};
        barrier_material_instance->SetScalarParameterValue(TEXT("Opacity"), opacity);

        // Add pulsing effect during transitions
        if (current_state == EForcefieldState::Activating ||
            current_state == EForcefieldState::Deactivating) {
            auto const pulse_time{static_cast<float>(GetWorld()->GetTimeSeconds())};
            auto const pulse_value{FMath::Sin(pulse_time * 5.0f) * 0.5f + 0.5f};
            barrier_material_instance->SetScalarParameterValue(TEXT("EmissiveStrength"),
                                                               pulse_value * 2.0f);
        } else {
            barrier_material_instance->SetScalarParameterValue(TEXT("EmissiveStrength"), 1.0f);
        }
    }
}

void AForcefieldActor::update_collision_blocking() {
    bool const should_block{current_state == EForcefieldState::Active};

    if (should_block) {
        collision_volume->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        collision_volume->SetCollisionResponseToAllChannels(ECR_Block);
    } else {
        collision_volume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}

void AForcefieldActor::draw_debug_info() const {
    if (!GetWorld()) {
        return;
    }

    FVector const center{GetActorLocation()};
    FVector const extent{collision_volume->GetScaledBoxExtent()};

    // Draw collision box
    FColor debug_color;
    switch (current_state) {
        case EForcefieldState::Inactive:
            debug_color = FColor::Silver;
            break;
        case EForcefieldState::Activating:
            debug_color = FColor::Yellow;
            break;
        case EForcefieldState::Active:
            debug_color = FColor::Red;
            break;
        case EForcefieldState::Deactivating:
            debug_color = FColor::Orange;
            break;
        case EForcefieldState::Cooldown:
            debug_color = FColor::Blue;
            break;
        default:
            debug_color = FColor::Magenta;
            break;
    }

    DrawDebugBox(GetWorld(),
                 center,
                 extent,
                 debug_color,
                 debug_persistent_lines,
                 debug_lifetime,
                 debug_depth_priority,
                 debug_line_thickness);

    // Draw state text
    FString const state_text{
        FString::Printf(TEXT("Forcefield: %s"), *UEnum::GetValueAsString(current_state))};
    DrawDebugString(GetWorld(),
                    center + FVector{0, 0, extent.Z + 50.0f},
                    state_text,
                    nullptr,
                    debug_color,
                    debug_lifetime);
}

bool AForcefieldActor::is_in_transition_state() const {
    return current_state == EForcefieldState::Activating ||
           current_state == EForcefieldState::Deactivating;
}

bool AForcefieldActor::can_change_state() const {
    return current_state == EForcefieldState::Inactive || current_state == EForcefieldState::Active;
}
