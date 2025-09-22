#include "ForcefieldActor.h"

#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sandbox/subsystems/TriggerSubsystem.h"
#include "TimerManager.h"

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

    transition_pulse_timeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("PulseTimeline"));
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

    if (transition_pulse_timeline && transition_pulse_curve) {
        FOnTimelineFloat timeline_callback;
        timeline_callback.BindUFunction(this, FName("on_pulse_update"));
        transition_pulse_timeline->AddInterpFloat(transition_pulse_curve, timeline_callback);

        FOnTimelineEvent timeline_finished;
        timeline_finished.BindUFunction(this, FName("on_pulse_end"));
        transition_pulse_timeline->SetTimelineFinishedFunc(timeline_finished);

        transition_pulse_timeline->SetLooping(false);
        transition_pulse_timeline->SetIgnoreTimeDilation(true);
    } else {
        log_warning(TEXT("Transition pulse timeline or curve missing"));
    }

    initialise_barrier_material();

    change_state(EForcefieldState::Active);
    if (current_state == EForcefieldState::Active) {
        set_emissive_strength(get_curve_end_value());
        transition_pulse_timeline->SetPlaybackPosition(
            transition_pulse_timeline->GetTimelineLength(), false);
    } else if (current_state == EForcefieldState::Inactive) {
        set_emissive_strength(get_curve_start_value());
        transition_pulse_timeline->SetPlaybackPosition(0, false);
    }

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

    // Register with TriggerSubsystem
    if (auto* const trigger_subsystem{GetWorld()->GetSubsystem<UTriggerSubsystem>()}) {
        if (auto const id{trigger_subsystem->register_triggerable(*this, forcefield_payload)}) {
            triggerable_id = *id;
            log_verbose(TEXT("Registered forcefield with trigger subsystem"));
        } else {
            log_error(TEXT("Failed to register forcefield with trigger subsystem"));
            return;
        }
    }

    log_verbose(TEXT("Forcefield initialized at %s"), *GetActorLocation().ToString());
}

void AForcefieldActor::trigger_activation() {
    if (!can_activate()) {
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
            log_verbose(TEXT("Ignoring activation - currently in state: %s"),
                        *to_fstring(current_state));
            break;
        }
    }
}

bool AForcefieldActor::can_activate() const {
    return current_state == EForcefieldState::Inactive || current_state == EForcefieldState::Active;
}

void AForcefieldActor::start_activation() {
    change_state(EForcefieldState::Activating);

    // Clear any existing timers
    GetWorldTimerManager().ClearTimer(state_timer_handle);
    GetWorldTimerManager().ClearTimer(cooldown_timer_handle);

    // Start visual effects immediately
    update_visual_effects();

    // Start timeline from beginning
    if (transition_pulse_timeline) {
        transition_pulse_timeline->PlayFromStart();
    }

    log_verbose(TEXT("Activation started - timeline will handle completion"));
}

void AForcefieldActor::start_deactivation() {
    change_state(EForcefieldState::Deactivating);

    GetWorldTimerManager().ClearTimer(state_timer_handle);
    GetWorldTimerManager().ClearTimer(cooldown_timer_handle);

    update_collision_blocking();

    if (transition_pulse_timeline) {
        transition_pulse_timeline->Reverse();
    }

    log_verbose(TEXT("Deactivation started - timeline will handle completion"));
}

void AForcefieldActor::start_cooldown() {
    change_state(EForcefieldState::Cooldown);

    // Set timer to complete cooldown
    GetWorldTimerManager().SetTimer(cooldown_timer_handle,
                                    this,
                                    &AForcefieldActor::complete_cooldown,
                                    cooldown_duration,
                                    false);

    log_verbose(TEXT("Cooldown started - will complete in %.1f seconds"), cooldown_duration);
}

void AForcefieldActor::complete_cooldown() {
    change_state(EForcefieldState::Inactive);

    log_verbose(TEXT("Cooldown completed - forcefield ready for activation"));
}
void AForcefieldActor::change_state(EForcefieldState state) {
    current_state = state;
    log_verbose(TEXT("Changing state to %s"), *to_fstring(state));
}
void AForcefieldActor::update_visual_effects() {
    bool const should_be_visible{is_visible()};

    // Update barrier mesh visibility
    barrier_mesh->SetVisibility(should_be_visible);

    // Update particle effects
    if (should_be_visible && !sparkle_particles->IsActive()) {
        sparkle_particles->Activate();
    } else if (!should_be_visible && sparkle_particles->IsActive()) {
        sparkle_particles->Deactivate();
    }

    // Update distortion effect
    if (distortion_effect) {
        distortion_effect->bEnabled = should_be_visible;
        if (distortion_effect->Settings.WeightedBlendables.Array.Num() > 0) {
            distortion_effect->Settings.WeightedBlendables.Array[0].Weight =
                should_be_visible ? distortion_strength : 0.0f;
        }
    }

    // Update material parameters
    if (barrier_material_instance) {
        // Set base noise parameters (will be scaled by set_emissive_strength)
        barrier_material_instance->SetScalarParameterValue(Constants::NoiseSpeed(),
                                                           noise_animation_speed);
        barrier_material_instance->SetScalarParameterValue(Constants::NoiseIntensity(),
                                                           noise_intensity);
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
void AForcefieldActor::on_pulse_update(float value) {
    set_emissive_strength(value);
}

void AForcefieldActor::on_pulse_end() {
    if (current_state == EForcefieldState::Activating) {
        set_emissive_strength(get_curve_end_value());
        change_state(EForcefieldState::Active);
        update_collision_blocking();
        log_verbose(TEXT("Forcefield activation completed - now blocking"));
    } else if (current_state == EForcefieldState::Deactivating) {
        set_emissive_strength(get_curve_start_value());
        change_state(EForcefieldState::Inactive);
        update_visual_effects();
        start_cooldown();
        log_verbose(TEXT("Forcefield deactivation completed - starting cooldown"));
    }
}
void AForcefieldActor::initialise_barrier_material() {
    if (!barrier_mesh) {
        return;
    }

    // Create dynamic material instance for the barrier
    if (auto* current_material{barrier_mesh->GetMaterial(0)}) {
        barrier_material_instance =
            UMaterialInstanceDynamic::Create(barrier_mesh->GetMaterial(0), this);
        if (barrier_material_instance) {
            barrier_mesh->SetMaterial(0, barrier_material_instance);

            barrier_material_instance->SetVectorParameterValue(Constants::ForwardVector(),
                                                               barrier_mesh->GetRightVector());

            // Initialize resolved peak opacity
            if (peak_opacity < 0.0f) {
                // Use material's default opacity value
                float material_opacity{default_opacity}; // fallback
                if (barrier_material_instance->GetScalarParameterValue(Constants::Opacity(),
                                                                       material_opacity)) {
                    resolved_peak_opacity = material_opacity;
                    log_verbose(TEXT("Using material default opacity: %f"), resolved_peak_opacity);
                } else {
                    resolved_peak_opacity = default_opacity;
                    log_verbose(TEXT("Material has no opacity parameter, using fallback: %f"),
                                resolved_peak_opacity);
                }
            } else {
                // Use override value
                resolved_peak_opacity = peak_opacity;
                log_verbose(TEXT("Using override opacity: %f"), resolved_peak_opacity);
            }
        }
    }
}
float AForcefieldActor::get_curve_start_value() const {
    if (transition_pulse_curve) {
        float min_time, max_time;
        transition_pulse_curve->GetTimeRange(min_time, max_time);
        return transition_pulse_curve->GetFloatValue(min_time);
    }
    return 0.0f;
}

float AForcefieldActor::get_curve_end_value() const {
    if (transition_pulse_curve) {
        float min_time, max_time;
        transition_pulse_curve->GetTimeRange(min_time, max_time);
        return transition_pulse_curve->GetFloatValue(max_time);
    }
    return 1.0f;
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
    FString const state_text{FString::Printf(TEXT("Forcefield: %s"), *to_fstring(current_state))};
    DrawDebugString(GetWorld(),
                    center + FVector{0, 0, extent.Z + 50.0f},
                    state_text,
                    nullptr,
                    debug_color,
                    debug_lifetime);
}

bool AForcefieldActor::is_visible() const {
    return current_state == EForcefieldState::Activating ||
           current_state == EForcefieldState::Active ||
           current_state == EForcefieldState::Deactivating;
}
bool AForcefieldActor::is_in_transition_state() const {
    return current_state == EForcefieldState::Activating ||
           current_state == EForcefieldState::Deactivating;
}

bool AForcefieldActor::can_change_state() const {
    return current_state == EForcefieldState::Inactive || current_state == EForcefieldState::Active;
}
void AForcefieldActor::set_emissive_strength(float es) {
    if (barrier_material_instance) {
        log_verbose(TEXT("Setting EmissiveStrength to %f."), es);
        barrier_material_instance->SetScalarParameterValue(Constants::EmissiveStrength(), es);

        // Scale opacity with timeline value (0.0 → resolved_peak_opacity)
        float const scaled_opacity{es * resolved_peak_opacity};
        barrier_material_instance->SetScalarParameterValue(Constants::Opacity(), scaled_opacity);

        // Scale noise animation with activation state
        float const scaled_noise_speed{noise_animation_speed * es};
        barrier_material_instance->SetScalarParameterValue(Constants::NoiseSpeed(),
                                                           scaled_noise_speed);

        // Scale noise intensity with activation state
        float const scaled_noise_intensity{noise_intensity * es};
        barrier_material_instance->SetScalarParameterValue(Constants::NoiseIntensity(),
                                                           scaled_noise_intensity);
    }
}
void AForcefieldActor::set_opacity(float opacity) {
    log_verbose(TEXT("Setting opacity to %f."), opacity);
    barrier_material_instance->SetScalarParameterValue(Constants::Opacity(), opacity);
}
