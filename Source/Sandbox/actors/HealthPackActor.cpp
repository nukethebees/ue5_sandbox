// Fill out your copyright notice in the Description page of Project Settings.

#include "Sandbox/actors/HealthPackActor.h"

#include "Sandbox/actor_components/HealthComponent.h"
#include "Sandbox/subsystems/DamageManagerSubsystem.h"

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
    rotating_component->speed = 100.0f;
}

void AHealthPackActor::BeginPlay() {
    Super::BeginPlay();

    if (collision_component) {
        collision_component->OnComponentBeginOverlap.AddDynamic(
            this, &AHealthPackActor::on_overlap_begin);
    }
}

void AHealthPackActor::on_overlap_begin(UPrimitiveComponent* OverlappedComponent,
                                        AActor* OtherActor,
                                        UPrimitiveComponent* OtherComponent,
                                        int32 OtherBodyIndex,
                                        bool bFromSweep,
                                        FHitResult const& SweepResult) {
    if (!OtherActor) {
        log_warning(TEXT("No OtherActor in overlap"));
        return;
    }

    auto* health_component{OtherActor->FindComponentByClass<UHealthComponent>()};
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
