#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sandbox/logging/mixins/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "WeaponPickup.generated.h"

class URotatingActorComponent;
class UStaticMeshComponent;
class UBoxComponent;
class AWeaponBase;

UCLASS()
class SANDBOX_API AWeaponPickup
    : public AActor
    , public ml::LogMsgMixin<"AWeaponPickup", LogSandboxWeapon> {
    GENERATED_BODY()
  public:
    AWeaponPickup();
  protected:
    virtual void OnConstruction(FTransform const& Transform) override;
    virtual void BeginPlay() override;

    void spawn_weapon(UWorld& world);
    void update_weapon(UWorld& world);
    void attach_weapon();
    UFUNCTION()
    void on_overlap_begin(UPrimitiveComponent* overlapped_component,
                          AActor* other_actor,
                          UPrimitiveComponent* other_component,
                          int32 other_body_index,
                          bool from_sweep,
                          FHitResult const& sweep_result);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    TSubclassOf<AWeaponBase> weapon_class{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    UBoxComponent* collision_box{nullptr};
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    URotatingActorComponent* rotator{nullptr};
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    AWeaponBase* spawned_weapon{nullptr};

    FActorSpawnParameters spawn_parameters{};
};
