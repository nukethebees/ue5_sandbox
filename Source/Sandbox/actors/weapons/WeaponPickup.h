#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "WeaponPickup.generated.h"

class URotatingActorComponent;
class UStaticMeshComponent;
class UBoxComponent;

UCLASS()
class SANDBOX_API AWeaponPickup : public AActor {
    GENERATED_BODY()
  public:
    AWeaponPickup();
  protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void on_overlap_begin(UPrimitiveComponent* OverlappedComponent,
                          AActor* OtherActor,
                          UPrimitiveComponent* OtherComponent,
                          int32 OtherBodyIndex,
                          bool bFromSweep,
                          FHitResult const& SweepResult);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    UStaticMeshComponent* weapon_mesh{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    UBoxComponent* collision_component{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    URotatingActorComponent* rotation_component{nullptr};
};
