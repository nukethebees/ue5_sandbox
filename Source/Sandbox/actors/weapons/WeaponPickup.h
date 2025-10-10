#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sandbox/mixins/log_msg_mixin.hpp"
#include "Sandbox/SandboxLogCategories.h"

#include "WeaponPickup.generated.h"

class URotatingActorComponent;
class UStaticMeshComponent;
class UBoxComponent;
class AWeaponBase;

UCLASS()
class SANDBOX_API AWeaponPickup
    : public AActor
    , public ml::LogMsgMixin<"AWeaponPickup", LogSandboxGuns> {
    GENERATED_BODY()
  public:
    AWeaponPickup();
  protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void on_overlap_begin(UPrimitiveComponent* overlapped_component,
                          AActor* other_actor,
                          UPrimitiveComponent* other_component,
                          int32 other_body_index,
                          bool from_sweep,
                          FHitResult const& sweep_result);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    TSubclassOf<AWeaponBase> weapon_class{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    UStaticMeshComponent* weapon_mesh{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    UBoxComponent* collision_box{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
    URotatingActorComponent* rotator{nullptr};
};
