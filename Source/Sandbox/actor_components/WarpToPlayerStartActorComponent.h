#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WarpToPlayerStartActorComponent.generated.h"

class UBoxComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UWarpToPlayerStartActorComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UWarpToPlayerStartActorComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    UBoxComponent* warp_zone{nullptr};
  protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp,
                        AActor* OtherActor,
                        UPrimitiveComponent* OtherComp,
                        int32 OtherBodyIndex,
                        bool bFromSweep,
                        FHitResult const& SweepResult);
};
