#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"

#include "Sandbox/combat/projectiles/data/generated/BulletTypeIndex.h"
#include "Sandbox/combat/weapons/actors/WeaponBase.h"
#include "Sandbox/environment/utilities/world.h"
#include "Sandbox/interaction/interfaces/Describable.h"
#include "Sandbox/logging/SandboxLogCategories.h"
#include "Sandbox/utilities/string_literal_wrapper.h"

#include "MassBulletWeapon.generated.h"

class UStaticMeshComponent;
class UArrowComponent;
class UBulletDataAsset;

UCLASS(Abstract)
class SANDBOX_API AMassBulletWeapon : public AWeaponBase {
    GENERATED_BODY()
  public:
    static constexpr StaticTCharString tag{"AMassBulletWeapon"};
  protected:
    virtual void BeginPlay() override;

#if WITH_EDITOR
    virtual void OnConstruction(FTransform const& Transform) override;

    template <typename T>
    static T* ensure_actor_singleton(UWorld& world) {
        if (auto* actor{ml::get_first_actor<T>(world)}) {
            return actor;
        }

        FActorSpawnParameters spawn_params{};
        auto* actor{world.SpawnActor<T>(FVector::ZeroVector, FRotator::ZeroRotator, spawn_params)};

        if (actor) {
            actor->SetActorLabel(T::StaticClass()->GetName());
        }

        return actor;
    }
#endif

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gun")
    TObjectPtr<UBulletDataAsset> bullet_data{nullptr};

    TOptional<FBulletTypeIndex> cached_bullet_type_index;
};
