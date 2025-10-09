#include "Sandbox/actor_components/WarpToPlayerStartActorComponent.h"

#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"

UWarpToPlayerStartActorComponent::UWarpToPlayerStartActorComponent() {
    PrimaryComponentTick.bCanEverTick = false;
}

void UWarpToPlayerStartActorComponent::BeginPlay() {
    Super::BeginPlay();

    if (warp_zone) {
        warp_zone->OnComponentBeginOverlap.AddDynamic(
            this, &UWarpToPlayerStartActorComponent::OnOverlapBegin);
    } else {
        UE_LOG(LogTemp, Warning, TEXT("warp_zone is null in UWarpToPlayerStartActorComponent"));
    }
}

void UWarpToPlayerStartActorComponent::OnOverlapBegin(UPrimitiveComponent* OverlappedComp,
                                                      AActor* other_actor,
                                                      UPrimitiveComponent* OtherComp,
                                                      int32 other_body_index,
                                                      bool from_sweep,
                                                      FHitResult const& sweep_result) {
    UE_LOG(LogTemp, Warning, TEXT("Overlap begin"));
    for (auto it{TActorIterator<APlayerStart>(GetWorld())}; it; ++it) {
        if (other_actor) {
            other_actor->SetActorLocation(it->GetActorLocation());
        }
        return;
    }
    UE_LOG(LogTemp, Warning, TEXT("UWarpToPlayerStartActorComponent: No player start found."));
}
