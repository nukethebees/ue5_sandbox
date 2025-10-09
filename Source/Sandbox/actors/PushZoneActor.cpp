#include "PushZoneActor.h"

#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"

APushZoneActor::APushZoneActor() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    // Create root scene component
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));

    // Create and setup collision box as child of root
    collision_box = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
    collision_box->SetupAttachment(RootComponent);

    // Set default box size (can be adjusted in Blueprint/editor)
    collision_box->SetBoxExtent(FVector{500.0f, 500.0f, 250.0f});

    // Configure collision
    collision_box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    collision_box->SetCollisionResponseToAllChannels(ECR_Ignore);
    collision_box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    collision_box->SetGenerateOverlapEvents(true);

    // Create outer visual mesh component
    visual_mesh_outer = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMeshOuter"));
    visual_mesh_outer->SetupAttachment(RootComponent);
    visual_mesh_outer->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Create inner visual mesh component
    visual_mesh_inner = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMeshInner"));
    visual_mesh_inner->SetupAttachment(RootComponent);
    visual_mesh_inner->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void APushZoneActor::OnConstruction(FTransform const& Transform) {
    Super::OnConstruction(Transform);

    static constexpr auto LOGGER{NestedLogger<"OnConstruction">()};

    LOGGER.log_verbose(TEXT("Start"));

    // Explicit null checks with error messages
    if (!collision_box) {
        LOGGER.log_error(TEXT("collision_box is null - this should never happen"));
        return;
    }

    if (!visual_mesh_outer) {
        LOGGER.log_error(TEXT("visual_mesh_outer is null - this should never happen"));
        return;
    }

    if (!visual_mesh_inner) {
        LOGGER.log_error(TEXT("visual_mesh_inner is null - this should never happen"));
        return;
    }

    if (!visual_mesh_outer->GetStaticMesh()) {
        LOGGER.log_error(TEXT("visual_mesh_outer has no static mesh - set mesh in Blueprint"));
        return;
    }

    // Position and scale outer mesh to match collision box

    LOGGER.log_verbose(TEXT("Position and scale outer mesh"));

    // Match position and rotation of collision box
    visual_mesh_outer->SetWorldLocation(collision_box->GetComponentLocation());
    visual_mesh_outer->SetWorldRotation(collision_box->GetComponentRotation());

    // Scale to match collision box size
    FVector const collision_size{collision_box->GetScaledBoxExtent() * 2.0f};
    FVector const outer_mesh_size{visual_mesh_outer->GetStaticMesh()->GetBoundingBox().GetSize()};

    if (!outer_mesh_size.IsNearlyZero()) {
        FVector const desired_scale{collision_size / outer_mesh_size};
        visual_mesh_outer->SetWorldScale3D(desired_scale);
    }

    // Update inner mesh to match outer mesh
    // Copy mesh from outer shell
    visual_mesh_inner->SetStaticMesh(visual_mesh_outer->GetStaticMesh());

    // Match position and rotation of outer mesh
    visual_mesh_inner->SetWorldLocation(visual_mesh_outer->GetComponentLocation());
    visual_mesh_inner->SetWorldRotation(visual_mesh_outer->GetComponentRotation());

    // Scale inner shell smaller than outer
    FVector const inner_mesh_size{visual_mesh_inner->GetStaticMesh()->GetBoundingBox().GetSize()};

    if (!inner_mesh_size.IsNearlyZero()) {
        FVector const inner_collision_size{collision_size * inner_shell_scale_ratio};
        FVector const inner_desired_scale{inner_collision_size / inner_mesh_size};
        visual_mesh_inner->SetWorldScale3D(inner_desired_scale);
    }

    // Apply material with inner shell properties
    if (auto* base_material{visual_mesh_outer->GetMaterial(0)}) {
        auto* dynamic_material{UMaterialInstanceDynamic::Create(base_material, this)};
        dynamic_material->SetScalarParameterValue(TEXT("OpacityMultiplier"),
                                                  inner_opacity_multiplier);
        dynamic_material->SetVectorParameterValue(TEXT("ColorTint"), inner_color_tint);
        visual_mesh_inner->SetMaterial(0, dynamic_material);
    }
}

void APushZoneActor::BeginPlay() {
    Super::BeginPlay();

    // Bind overlap events
    collision_box->OnComponentBeginOverlap.AddDynamic(this, &APushZoneActor::on_overlap_begin);
    collision_box->OnComponentEndOverlap.AddDynamic(this, &APushZoneActor::on_overlap_end);
}

void APushZoneActor::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);

    // This should only be called by our custom timer, but just in case
    apply_push_forces();

    if (show_debug_visualization) {
        draw_debug_info();
    }
}

void APushZoneActor::on_overlap_begin(UPrimitiveComponent* overlapped_component,
                                      AActor* other_actor,
                                      UPrimitiveComponent* other_component,
                                      int32 other_body_index,
                                      bool from_sweep,
                                      FHitResult const& sweep_result) {
    if (!other_actor || other_actor == this) {
        log_warning(TEXT("Overlap begin - invalid actor or self-overlap"));
        return;
    }

    // Only affect pawns (characters, vehicles, etc.)
    if (!Cast<APawn>(other_actor)) {
        log_warning(TEXT("Overlap begin - actor %s is not a pawn, ignoring"),
                    *other_actor->GetName());
        return;
    }

    log_verbose(TEXT("Actor %s entered push zone"), *other_actor->GetName());

    // Add to our tracking list
    overlapping_actors.AddUnique(TWeakObjectPtr<AActor>(other_actor));

    // Start ticking if this is the first actor
    if (!is_zone_occupied) {
        start_custom_ticking();
    }
}

void APushZoneActor::on_overlap_end(UPrimitiveComponent* overlapped_component,
                                    AActor* other_actor,
                                    UPrimitiveComponent* other_component,
                                    int32 other_body_index) {
    if (!other_actor || other_actor == this) {
        return;
    }

    // Remove from tracking list
    overlapping_actors.RemoveAll([other_actor](TWeakObjectPtr<AActor> const& WeakActor) {
        return !WeakActor.IsValid() || WeakActor.Get() == other_actor;
    });

    // Stop ticking if no actors remain
    cleanup_invalid_actors();
    if (overlapping_actors.IsEmpty()) {
        stop_custom_ticking();
    }
}

void APushZoneActor::start_custom_ticking() {
    constexpr bool loop_timer{true};

    if (auto* world{GetWorld()}; world && !is_zone_occupied) {
        is_zone_occupied = true;
        world->GetTimerManager().SetTimer(
            tick_timer_handle,
            [this]() {
                apply_push_forces();
                if (show_debug_visualization) {
                    draw_debug_info();
                }
            },
            custom_tick_interval,
            loop_timer);
    }
}

void APushZoneActor::stop_custom_ticking() {
    if (auto* world{GetWorld()}; world && is_zone_occupied) {
        is_zone_occupied = false;
        world->GetTimerManager().ClearTimer(tick_timer_handle);
    }
}

void APushZoneActor::cleanup_invalid_actors() {
    overlapping_actors.RemoveAll(
        [](TWeakObjectPtr<AActor> const& WeakActor) { return !WeakActor.IsValid(); });
}

void APushZoneActor::apply_push_forces() {
    cleanup_invalid_actors();

    for (auto const& weak_actor : overlapping_actors) {
        if (auto* actor{weak_actor.Get()}) {
            auto const base_force{calculate_push_force(actor)};

            float multiplier{0.0f};
            switch (force_mode) {
                case EPushForceMode::ContinuousForce: {
                    multiplier = force_multiplier;
                    break;
                }
                case EPushForceMode::InstantImpulse: {
                    multiplier = impulse_multiplier;
                    break;
                }
                default: {
                    log_error(TEXT("Unhandled force mode: %d"), static_cast<int32>(force_mode));
                    return;
                }
            }

            auto const final_force{multiplier * base_force};
            apply_force_to_actor(actor, final_force);
        }
    }
}

FVector APushZoneActor::calculate_push_force(AActor* target_actor) const {
    if (!target_actor) {
        return FVector::ZeroVector;
    }

    auto const zone_center{collision_box->GetComponentLocation()};
    auto const actor_location{target_actor->GetActorLocation()};
    auto const direction_vector{actor_location - zone_center};

    auto const distance{direction_vector.Size()};

    // Prevent division by zero and ensure minimum distance
    auto const safe_distance{FMath::Max(distance, min_distance_threshold)};

    // Calculate force magnitude using inverse square law
    auto const force_magnitude{max_force_strength / (safe_distance * safe_distance)};

    // Return normalized direction with calculated magnitude
    if (direction_vector.IsNearlyZero()) {
        // If at exact center, push in a random direction
        FVector const rand_force_vector{
            FMath::RandRange(-1.0f, 1.0f), FMath::RandRange(-1.0f, 1.0f), 0.0f};
        return rand_force_vector.GetSafeNormal() * force_magnitude;
    }

    return direction_vector.GetSafeNormal() * force_magnitude;
}

void APushZoneActor::apply_force_to_actor(AActor* target_actor, FVector const& force) const {
    if (!target_actor || force.IsNearlyZero()) {
        log_warning(TEXT("Cannot apply force - invalid actor or zero force"));
        return;
    }

    // Try to apply force to character movement component first
    if (auto* character{Cast<ACharacter>(target_actor)}) {
        if (auto* movement_component{character->GetCharacterMovement()}) {
            if (force_mode == EPushForceMode::InstantImpulse) {
                movement_component->AddImpulse(force, true);
#if UE_BUILD_DEBUG
                log_log(TEXT("Applied impulse %s to character %s"),
                        *force.ToString(),
                        *target_actor->GetName());
#endif
            } else {
                movement_component->AddForce(force);
#if UE_BUILD_DEBUG
                log_log(TEXT("Applied force %s to character %s"),
                        *force.ToString(),
                        *target_actor->GetName());
#endif
            }
            return;
        } else {
            log_warning(TEXT("Character %s has no movement component"), *target_actor->GetName());
        }
    }

    // Fallback: try to apply to any primitive component with physics
    if (auto* primitive_component{target_actor->GetRootComponent()}) {
        if (auto* primitive{Cast<UPrimitiveComponent>(primitive_component)}) {
            if (primitive->IsSimulatingPhysics()) {
                if (force_mode == EPushForceMode::InstantImpulse) {
                    primitive->AddImpulse(force, NAME_None, true);
                    log_log(TEXT("Applied physics impulse %s to actor %s"),
                            *force.ToString(),
                            *target_actor->GetName());
                } else {
                    primitive->AddForce(force, NAME_None, true);
                    log_log(TEXT("Applied physics force %s to actor %s"),
                            *force.ToString(),
                            *target_actor->GetName());
                }
            } else {
                log_warning(TEXT("Actor %s root component not simulating physics"),
                            *target_actor->GetName());
            }
        } else {
            log_warning(TEXT("Actor %s root component is not a primitive component"),
                        *target_actor->GetName());
        }
    } else {
        log_warning(TEXT("Actor %s has no root component"), *target_actor->GetName());
    }
}

void APushZoneActor::draw_debug_info() const {
    auto* world{GetWorld()};
    if (!world) {
        return;
    }

    auto const zone_center{collision_box->GetComponentLocation()};
    auto const box_extent{collision_box->GetScaledBoxExtent()};

    // Draw zone bounds
    DrawDebugBox(world,
                 zone_center,
                 box_extent,
                 FColor::Yellow,
                 false,
                 custom_tick_interval + 0.1f,
                 0,
                 2.0f);

    // Draw center point
    DrawDebugSphere(
        world, zone_center, 50.0f, 12, FColor::Red, false, custom_tick_interval + 0.1f, 0, 2.0f);

    // Draw force vectors for each overlapping actor
    for (auto const& weak_actor : overlapping_actors) {
        if (auto* actor{weak_actor.Get()}) {
            auto const actor_location{actor->GetActorLocation()};
            auto const force{calculate_push_force(actor)};

            // Normalize force for visualization (scale it down)
            auto const visual_force{force.GetSafeNormal() * 200.0f};

            DrawDebugDirectionalArrow(world,
                                      actor_location,
                                      actor_location + visual_force,
                                      50.0f,
                                      FColor::Green,
                                      false,
                                      custom_tick_interval + 0.1f,
                                      0,
                                      3.0f);
        }
    }
}
