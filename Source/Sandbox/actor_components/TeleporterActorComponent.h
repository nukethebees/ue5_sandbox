#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TeleporterActorComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UTeleporterActorComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UTeleporterActorComponent();
  protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void HandleOverlap(UPrimitiveComponent* OverlappedComp,
                       AActor* OtherActor,
                       UPrimitiveComponent* OtherComp,
                       int32 OtherBodyIndex,
                       bool bFromSweep,
                       FHitResult const& SweepResult);
  public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teleport")
    UPrimitiveComponent* collision_trigger;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teleport")
    USceneComponent* target_location;
};
