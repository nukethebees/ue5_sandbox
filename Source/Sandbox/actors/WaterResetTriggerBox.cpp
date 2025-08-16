#include "Sandbox/actors/WaterResetTriggerBox.h"

#include "EngineUtils.h"
#include "GameFramework/Character.h"

void AWaterResetTriggerBox::BeginPlay() {
    Super::BeginPlay();

    for (auto it{TActorIterator<APlayerStart>(GetWorld())}; it; ++it) {
        player_start = *it;
        break;
    }

    OnActorBeginOverlap.AddDynamic(this, &AWaterResetTriggerBox::OnOverlapBegin);
}
void AWaterResetTriggerBox::OnOverlapBegin(AActor* OverlappedActor, AActor* OtherActor) {
    if (player_start) {
        if (auto* character{Cast<ACharacter>(OtherActor)}) {
            character->SetActorLocation(player_start->GetActorLocation());
        }
    }
}
