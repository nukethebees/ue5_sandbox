#include "Sandbox/environment/traversal/actor_components/AntigravLiftActorComponent.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UAntigravLiftActorComponent::UAntigravLiftActorComponent() {
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UAntigravLiftActorComponent::BeginPlay() {
    Super::BeginPlay();

    SetComponentTickEnabled(false);

    if (lift_zone) {
        lift_zone->OnComponentBeginOverlap.AddDynamic(this,
                                                      &UAntigravLiftActorComponent::OnOverlapBegin);
        lift_zone->OnComponentEndOverlap.AddDynamic(this,
                                                    &UAntigravLiftActorComponent::OnOverlapEnd);
    } else {
        UE_LOG(LogTemp, Warning, TEXT("AntiGravityLiftComponent: lift_zone is not assigned."));
    }
}
void UAntigravLiftActorComponent::TickComponent(float DeltaTime,
                                                ELevelTick TickType,
                                                FActorComponentTickFunction* ThisTickFunction) {
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    static constexpr auto dampening{0.2f};

    auto const current_chars{floating_characters};
    float const lift_direction{is_antigravity ? 1.0f : -1.0f};

    for (auto* character : current_chars) {
        if (IsValid(character)) {
            auto* const movement{character->GetCharacterMovement()};
            if (movement && movement->MovementMode == MOVE_Flying) {

                movement->Velocity.X *= dampening;
                movement->Velocity.Y *= dampening;

                auto const current_location{character->GetActorLocation()};
                auto const lift_amount{lift_speed * DeltaTime * lift_direction};
                auto const new_location{current_location + FVector{0.0f, 0.0f, lift_amount}};
                character->SetActorLocation(new_location, true);
            }
        }
    }
}

void UAntigravLiftActorComponent::OnOverlapBegin(UPrimitiveComponent* OverlappedComp,
                                                 AActor* other_actor,
                                                 UPrimitiveComponent* OtherComp,
                                                 int32 other_body_index,
                                                 bool from_sweep,
                                                 FHitResult const& sweep_result) {

    if (auto* character{Cast<ACharacter>(other_actor)}) {
        floating_characters.Add(character);
        if (auto* movement{character->GetCharacterMovement()}) {
            movement->SetMovementMode(MOVE_Flying);
        }
    }

    if (!floating_characters.IsEmpty()) {
        SetComponentTickEnabled(true);
    }
}

void UAntigravLiftActorComponent::OnOverlapEnd(UPrimitiveComponent* OverlappedComp,
                                               AActor* other_actor,
                                               UPrimitiveComponent* OtherComp,
                                               int32 other_body_index) {
    if (auto* character{Cast<ACharacter>(other_actor)}) {
        floating_characters.Remove(character);
        if (auto* movement{character->GetCharacterMovement()}) {
            movement->SetMovementMode(MOVE_Walking);
        }
    }

    if (floating_characters.IsEmpty()) {
        SetComponentTickEnabled(false);
    }
}

void UAntigravLiftActorComponent::display_n_floating() {
    UE_LOG(LogTemp,
           Display,
           TEXT("AntiGravityLiftComponent: FloatingCharacters count = %d"),
           floating_characters.Num());
}
