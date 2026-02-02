#include "Sandbox/combat/explosion/ExplosionSubsystem.h"

#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Sandbox/environment/effects/subsystems/NiagaraNdcWriterSubsystem.h"
#include "Sandbox/health/HealthComponent.h"
#include "Sandbox/health/DamageManagerSubsystem.h"
#include "Sandbox/utilities/math.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UExplosionSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);
}

void UExplosionSubsystem::spawn_explosion(FVector location,
                                          FRotator rotation,
                                          FExplosionConfig const& config) {
    static constexpr auto logger{NestedLogger<"spawn_explosion">()};

    RETURN_IF_NULLPTR(config.explosion_channel);

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(ndc_manager, world->GetSubsystem<UNiagaraNdcWriterSubsystem>());

    auto const writer_index{ndc_manager->register_asset(*config.explosion_channel)};

    execute_explosion(location, rotation, config, writer_index);
}

void UExplosionSubsystem::execute_explosion(FVector location,
                                            FRotator rotation,
                                            FExplosionConfig const& config,
                                            FNdcWriterIndex writer_index) {
    static constexpr auto logger{NestedLogger<"execute_explosion">()};

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(ndc_subsystem, world->GetSubsystem<UNiagaraNdcWriterSubsystem>());

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
    logger.log_verbose(TEXT("Spawning explosion at %s."), *location.ToString());
#endif
    ndc_subsystem->add_payload(writer_index, location, rotation.Vector());

    // Find all overlapping actors within explosion radius
    TArray<FOverlapResult> overlaps;
    FCollisionQueryParams query_params;
    query_params.bTraceComplex = false;
    query_params.bReturnPhysicalMaterial = false;

    FCollisionShape sphere{FCollisionShape::MakeSphere(config.explosion_radius)};

    bool found_overlaps{world->OverlapMultiByChannel(
        overlaps, location, FQuat::Identity, ECC_Pawn, sphere, query_params)};

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
    {
        constexpr bool persistent_lines{false};
        constexpr int32 segments{16};
        constexpr auto lifetime{5.0f};
        constexpr uint8 depth_priority{0};
        constexpr auto thickness{2.0f};
        DrawDebugSphere(world,
                        location,
                        config.explosion_radius,
                        segments,
                        FColor::Orange,
                        persistent_lines,
                        lifetime,
                        depth_priority,
                        thickness);
    }
#endif

    if (!found_overlaps) {
        return;
    }
    TRY_INIT_PTR(damage_manager, world->GetSubsystem<UDamageManagerSubsystem>());

    constexpr auto is_movable{[](auto& component) -> bool {
        return component.IsSimulatingPhysics() &&
               (component.Mobility == EComponentMobility::Movable);
    }};
    using CharMvmt = UCharacterMovementComponent;

    // Process each overlapping actor
    for (auto const& overlap : overlaps) {
        auto* target_actor{overlap.GetActor()};
        CONTINUE_IF_FALSE(target_actor);

        auto const distance{
            static_cast<float>(FVector::Dist(location, target_actor->GetActorLocation()))};

        // Apply damage if actor has health component
        if (auto* health_component{target_actor->FindComponentByClass<UHealthComponent>()}) {
            float const scaled_damage{ml::calculate_explosion_damage(
                distance, config.explosion_radius, config.damage_config.value)};
            if (scaled_damage > 0.0f) {
                FHealthChange damage_change{scaled_damage, config.damage_config.type};
                damage_manager->queue_health_change(health_component, damage_change);
            }
        }

        // Apply explosion force
        if (auto* movement_component{target_actor->FindComponentByClass<CharMvmt>()}) {
            movement_component->SetMovementMode(MOVE_Falling);
            FVector impulse{calculate_impulse(location,
                                              target_actor->GetActorLocation(),
                                              distance,
                                              config.explosion_radius,
                                              config.explosion_force)};
            movement_component->AddImpulse(impulse, true);
        } else if (auto* root_component{target_actor->GetRootComponent()}) {
            if (auto* primitive_component{Cast<UPrimitiveComponent>(root_component)}) {
                if (is_movable(*primitive_component)) {
                    FVector const impulse{calculate_impulse(location,
                                                            target_actor->GetActorLocation(),
                                                            distance,
                                                            config.explosion_radius,
                                                            config.explosion_force)};

                    primitive_component->AddImpulse(impulse, NAME_None, true);
                }
            }
        }
    }
}

FVector UExplosionSubsystem::calculate_impulse(FVector explosion_location,
                                               FVector target_location,
                                               float target_distance,
                                               float explosion_radius,
                                               float explosion_force) {
    FVector direction{target_location - explosion_location};
    direction.Z = FMath::Abs(direction.Z); // Force upward component
    direction.Normalize();

    float const force_scale{1.0f - (target_distance / explosion_radius)};
    FVector const impulse{direction * explosion_force * force_scale};

    return impulse;
}
