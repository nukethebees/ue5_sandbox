#include "Sandbox/environment/traversal/HorizAntigravLiftComponent.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Sandbox/players/MyCharacter.h"

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

                // Nudge character towards the centre so they're less likely to get
                // their feet stuck on geometry
                auto const zone_centre{float_zone->GetComponentLocation()};
                auto const to_centre{zone_centre - current_location};
                auto const centre_strength{10.0f};

                auto const center_nudge{to_centre.GetSafeNormal() * centre_strength * DeltaTime};

                auto const float_step{float_direction * move_amount};
                auto const new_location{current_location + float_step + center_nudge};

                character->SetActorLocation(new_location, true);
            }
        }
    }
}

void UHorizAntigravLiftComponent::OnOverlapBegin(UPrimitiveComponent* OverlappedComp,
                                                 AActor* other_actor,
                                                 UPrimitiveComponent* OtherComp,
                                                 int32 other_body_index,
                                                 bool from_sweep,
                                                 FHitResult const& sweep_result) {
    if (auto* character{Cast<AMyCharacter>(other_actor)}) {
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
                                               AActor* other_actor,
                                               UPrimitiveComponent* OtherComp,
                                               int32 other_body_index) {
    if (auto* character{Cast<AMyCharacter>(other_actor)}) {
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
