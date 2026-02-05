// Fill out your copyright notice in the Description page of Project Settings.

#include "Sandbox/health/HealthPackActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

#include "Sandbox/health/DamageManagerSubsystem.h"
#include "Sandbox/health/HealthComponent.h"

AHealthPackActor::AHealthPackActor() {
    PrimaryActorTick.bCanEverTick = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent->SetMobility(EComponentMobility::Movable);

    collision_component = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionComponent"));
    collision_component->SetupAttachment(RootComponent);
    collision_component->SetBoxExtent(FVector{50.0f, 50.0f, 50.0f});
    collision_component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    collision_component->SetCollisionResponseToAllChannels(ECR_Ignore);
    collision_component->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    mesh_component = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    mesh_component->SetupAttachment(RootComponent);
    mesh_component->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    rotating_component = CreateDefaultSubobject<URotatingActorComponent>(TEXT("RotatingComponent"));
    rotating_component->rotation_speed.Yaw = 100.f;
}

void AHealthPackActor::BeginPlay() {
    Super::BeginPlay();

    if (collision_component) {
        collision_component->OnComponentBeginOverlap.AddDynamic(
            this, &AHealthPackActor::on_overlap_begin);
    }
}

void AHealthPackActor::on_overlap_begin(UPrimitiveComponent* overlapped_component,
                                        AActor* other_actor,
                                        UPrimitiveComponent* other_component,
                                        int32 other_body_index,
                                        bool from_sweep,
                                        FHitResult const& sweep_result) {
    if (!other_actor) {
        log_warning(TEXT("No other_actor in overlap"));
        return;
    }

    auto* health_component{other_actor->FindComponentByClass<UHealthComponent>()};
    if (!health_component) {
        log_warning(TEXT("Actor has no HealthComponent, not consuming health pack"));
        return;
    }

    if (health_component->at_max_health() && (healing.type == EHealthChangeType::Healing)) {
        log_warning(TEXT("Actor already at max health, not consuming health pack"));
        return;
    }

    if (auto* damage_manager{GetWorld()->GetSubsystem<UDamageManagerSubsystem>()}) {
        damage_manager->queue_health_change(health_component, healing);
        Destroy();
    } else {
        log_error(TEXT("Could not get DamageManagerSubsystem"));
    }
}
