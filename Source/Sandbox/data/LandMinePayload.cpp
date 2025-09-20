#include "Sandbox/data/LandMinePayload.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Sandbox/actor_components/HealthComponent.h"
#include "Sandbox/data/HealthChange.h"
#include "Sandbox/subsystems/DamageManagerSubsystem.h"
#include "Sandbox/subsystems/DestructionManagerSubsystem.h"
#include "Sandbox/utilities/math.h"

void FLandMinePayload::execute(FCollisionContext context) {
    // Start timer to trigger explosion after delay
    if (detonation_delay > 0.0f) {
        context.world.GetTimerManager().SetTimer(
            timer_handle, [this, context]() { explode(context); }, detonation_delay, false);
    } else {
        // No delay, explode immediately
        explode(context);
    }
}

void FLandMinePayload::explode(FCollisionContext context) {
    static constexpr auto logger{log.NestedLogger<"execute">()};

    auto& world{context.world};
    auto& collided_actor{context.collided_actor};

    // Find all overlapping actors within explosion radius
    TArray<FOverlapResult> overlaps;
    FCollisionQueryParams query_params;
    query_params.bTraceComplex = false;
    query_params.bReturnPhysicalMaterial = false;

    // Use collision shape to find all actors in explosion radius
    FCollisionShape sphere{FCollisionShape::MakeSphere(explosion_radius)};

    bool found_overlaps{world.OverlapMultiByChannel(
        overlaps, mine_location, FQuat::Identity, ECC_Pawn, sphere, query_params)};

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
    {
        constexpr bool persistent_lines{true};
        constexpr int32 segments{8};
        constexpr auto lifetime{5.0f};
        constexpr uint8 depth_priority{0};
        constexpr auto thickness{1.0f};
        DrawDebugSphere(&world,
                        mine_location,
                        explosion_radius,
                        segments,
                        FColor::Yellow,
                        persistent_lines,
                        lifetime,
                        depth_priority,
                        thickness);
    }
#endif

    if (!found_overlaps) {
        logger.log_warning(TEXT("No overlaps found."));
        return;
    }

    auto* damage_manager{world.GetSubsystem<UDamageManagerSubsystem>()};
    if (!damage_manager) {
        logger.log_warning(TEXT("No UDamageManagerSubsystem found."));
        return;
    }

    // Process each overlapping actor
    for (auto const& overlap : overlaps) {
        auto* target_actor{overlap.GetActor()};
        if (!target_actor) {
            logger.log_warning(TEXT("target_actor is null."));
            continue;
        }

        logger.log_verbose(TEXT("Actor is %s"), *target_actor->GetName());

        // Calculate distance from mine (needed for scaling)
        auto const distance{
            static_cast<float>(FVector::Dist(mine_location, target_actor->GetActorLocation()))};

        // Apply damage if actor has health component
        if (auto* health_component{target_actor->FindComponentByClass<UHealthComponent>()}) {
            float const scaled_damage{
                ml::calculate_explosion_damage(distance, explosion_radius, damage)};
            if (scaled_damage > 0.0f) {
                FHealthChange damage_change{scaled_damage, EHealthChangeType::Damage};
                damage_manager->queue_damage(health_component, damage_change);
            }
        }

        constexpr auto is_movable{[](auto& component) -> bool {
            return component.IsSimulatingPhysics() &&
                   (component.Mobility == EComponentMobility::Movable);
        }};

        // Apply explosion force
        using CharMvmt = UCharacterMovementComponent;
        if (auto* movement_component{target_actor->FindComponentByClass<CharMvmt>()}) {
            logger.log_verbose(TEXT("Actor has UCharacterMovementComponent."));

            // Set to falling mode to eliminate ground friction
            movement_component->SetMovementMode(MOVE_Falling);

            FVector impulse{calculate_impulse(target_actor->GetActorLocation(), distance)};
            movement_component->AddImpulse(impulse, true);
        } else if (auto* root_component{target_actor->GetRootComponent()}) {
            logger.log_verbose(TEXT("Got root component."));

            if (auto* primitive_component{Cast<UPrimitiveComponent>(root_component)}) {
                logger.log_verbose(TEXT("Got UPrimitiveComponent."));

                if (is_movable(*primitive_component)) {
                    logger.log_verbose(TEXT("Component is movable"));

                    FVector const impulse{
                        calculate_impulse(target_actor->GetActorLocation(), distance)};

                    primitive_component->AddImpulse(impulse, NAME_None, true);
                } else {
                    logger.log_verbose(TEXT("Component is not movable"));
                }
            }
        } else {
            logger.log_verbose(TEXT("Component is not movable"));
        }
    }
}

FVector FLandMinePayload::calculate_impulse(FVector target_location, float target_distance) {
    // Calculate direction from mine to target
    FVector direction{target_location - mine_location};
    direction.Z = FMath::Abs(direction.Z); // Force upward component
    direction.Normalize();

    // Scale force by distance (closer = more force)
    float const force_scale{1.0f - (target_distance / explosion_radius)};
    FVector const impulse{direction * explosion_force * force_scale};

    return impulse;
}
