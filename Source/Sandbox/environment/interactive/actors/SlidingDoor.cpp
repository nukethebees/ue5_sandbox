#include "Sandbox/environment/interactive/actors/SlidingDoor.h"

#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

ASlidingDoor::ASlidingDoor() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    // Create root component
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));

    // Create door mesh
    door_mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
    door_mesh->SetupAttachment(RootComponent);
    door_mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    // Create trigger volume
    trigger_volume = CreateDefaultSubobject<UCapsuleComponent>(TEXT("TriggerVolume"));
    trigger_volume->SetupAttachment(RootComponent);
    trigger_volume->SetCapsuleRadius(300.0f);
    trigger_volume->SetCapsuleHalfHeight(200.0f);

    // Configure collision for trigger
    trigger_volume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    trigger_volume->SetCollisionResponseToAllChannels(ECR_Ignore);
    trigger_volume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    trigger_volume->SetGenerateOverlapEvents(true);
}

void ASlidingDoor::BeginPlay() {
    Super::BeginPlay();

    // Store initial closed position
    closed_position = door_mesh->GetRelativeLocation();

    // Bind overlap events
    trigger_volume->OnComponentBeginOverlap.AddDynamic(this,
                                                       &ASlidingDoor::on_trigger_begin_overlap);
    trigger_volume->OnComponentEndOverlap.AddDynamic(this, &ASlidingDoor::on_trigger_end_overlap);
}

void ASlidingDoor::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);

    update_door_position(DeltaTime);
}

void ASlidingDoor::on_trigger_begin_overlap(UPrimitiveComponent* overlapped_component,
                                            AActor* other_actor,
                                            UPrimitiveComponent* other_component,
                                            int32 other_body_index,
                                            bool from_sweep,
                                            FHitResult const& sweep_result) {
    if (!other_actor || other_actor == this) {
        return;
    }

    actor_count++;
    update_door_state();
}

void ASlidingDoor::on_trigger_end_overlap(UPrimitiveComponent* overlapped_component,
                                          AActor* other_actor,
                                          UPrimitiveComponent* other_component,
                                          int32 other_body_index) {
    if (!other_actor || other_actor == this) {
        return;
    }

    actor_count = FMath::Max(0, actor_count - 1);
    update_door_state();
}

FVector ASlidingDoor::calculate_target_offset() const {
    RETURN_VALUE_IF_NULLPTR(door_mesh, FVector::ZeroVector);
    RETURN_VALUE_IF_NULLPTR(door_mesh->GetStaticMesh(), FVector::ZeroVector);

    auto const bounds{door_mesh->GetStaticMesh()->GetBoundingBox()};
    auto const bounds_size{bounds.GetSize()};

    switch (direction) {
        case ESlidingDoorDirection::Left: {
            return FVector{0.0f, bounds_size.Y, 0.0f};
        }
        case ESlidingDoorDirection::Right: {
            return FVector{0.0f, -bounds_size.Y, 0.0f};
        }
        case ESlidingDoorDirection::Up: {
            return FVector{0.0f, 0.0f, bounds_size.Z};
        }
        case ESlidingDoorDirection::Down: {
            return FVector{0.0f, 0.0f, -bounds_size.Z};
        }
        default: {
            return FVector::ZeroVector;
        }
    }
}

void ASlidingDoor::update_door_state() {
    if (actor_count > 0) {
        if (door_state == ESlidingDoorState::Closed || door_state == ESlidingDoorState::Closing) {
            door_state = ESlidingDoorState::Opening;
            SetActorTickEnabled(true);
        }
    } else {
        if (door_state == ESlidingDoorState::Open || door_state == ESlidingDoorState::Opening) {
            door_state = ESlidingDoorState::Closing;
            SetActorTickEnabled(true);
        }
    }
}

void ASlidingDoor::update_door_position(float DeltaTime) {
    auto const delta_progress{(open_speed * DeltaTime) / calculate_target_offset().Size()};

    switch (door_state) {
        case ESlidingDoorState::Opening: {
            current_progress = FMath::Clamp(current_progress + delta_progress, 0.0f, 1.0f);
            if (FMath::IsNearlyEqual(current_progress, 1.0f)) {
                door_state = ESlidingDoorState::Open;
                SetActorTickEnabled(false);
            }
            break;
        }
        case ESlidingDoorState::Closing: {
            current_progress = FMath::Clamp(current_progress - delta_progress, 0.0f, 1.0f);
            if (FMath::IsNearlyEqual(current_progress, 0.0f)) {
                door_state = ESlidingDoorState::Closed;
                SetActorTickEnabled(false);
            }
            break;
        }
        case ESlidingDoorState::Open:
        case ESlidingDoorState::Closed:
        default: {
            return;
        }
    }

    auto const target_offset{calculate_target_offset()};
    auto const new_position{closed_position + (target_offset * current_progress)};
    door_mesh->SetRelativeLocation(new_position);
}
