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
                                                      AActor* OtherActor,
                                                      UPrimitiveComponent* OtherComp,
                                                      int32 OtherBodyIndex,
                                                      bool bFromSweep,
                                                      FHitResult const& SweepResult) {
    UE_LOG(LogTemp, Warning, TEXT("Overlap begin"));
    for (auto it{TActorIterator<APlayerStart>(GetWorld())}; it; ++it) {
        if (OtherActor) {
            OtherActor->SetActorLocation(it->GetActorLocation());
        }
        return;
    }
    UE_LOG(LogTemp, Warning, TEXT("UWarpToPlayerStartActorComponent: No player start found."));
}
