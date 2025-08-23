#include "Sandbox/actor_components/HorizAntigravLiftComponent.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Sandbox/characters/MyCharacter.h"

UHorizAntigravLiftComponent::UHorizAntigravLiftComponent() {
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UHorizAntigravLiftComponent::BeginPlay() {
    Super::BeginPlay();

    if (float_zone) {
        float_zone->OnComponentBeginOverlap.AddDynamic(
            this, &UHorizAntigravLiftComponent::OnOverlapBegin);
        float_zone->OnComponentEndOverlap.AddDynamic(this,
                                                     &UHorizAntigravLiftComponent::OnOverlapEnd);
    }
}

void UHorizAntigravLiftComponent::TickComponent(float DeltaTime,
                                                ELevelTick TickType,
                                                FActorComponentTickFunction* ThisTickFunction) {
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    auto const characters_snapshot{floating_characters};

    for (auto* character : characters_snapshot) {
        if (IsValid(character)) {
            auto* movement = character->GetCharacterMovement();
            if (movement && movement->MovementMode == MOVE_Flying) {
                movement->Velocity = FVector::ZeroVector;

                auto const current_location{character->GetActorLocation()};
                auto const move_amount{float_speed * DeltaTime};
                auto const new_location{current_location + float_direction * move_amount};

                character->SetActorLocation(new_location, true);
            }
        }
    }
}

void UHorizAntigravLiftComponent::OnOverlapBegin(UPrimitiveComponent* OverlappedComp,
                                                 AActor* OtherActor,
                                                 UPrimitiveComponent* OtherComp,
                                                 int32 OtherBodyIndex,
                                                 bool bFromSweep,
                                                 FHitResult const& SweepResult) {
    if (auto* character{Cast<AMyCharacter>(OtherActor)}) {
        character->is_forced_movement = true;

        floating_characters.Add(character);
        if (auto* movement{character->GetCharacterMovement()}) {
            movement->SetMovementMode(MOVE_Flying);
        }
    }

    if (!floating_characters.IsEmpty()) {
        SetComponentTickEnabled(true);
    }
}
void UHorizAntigravLiftComponent::OnOverlapEnd(UPrimitiveComponent* OverlappedComp,
                                               AActor* OtherActor,
                                               UPrimitiveComponent* OtherComp,
                                               int32 OtherBodyIndex) {
    if (auto* character{Cast<AMyCharacter>(OtherActor)}) {
        character->is_forced_movement = false;
        floating_characters.Remove(character);
        if (auto* movement{character->GetCharacterMovement()}) {
            movement->SetMovementMode(MOVE_Walking);
        }
    }

    if (floating_characters.IsEmpty()) {
        SetComponentTickEnabled(false);
    }
}
