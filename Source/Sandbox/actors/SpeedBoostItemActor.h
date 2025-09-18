#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Sandbox/interfaces/PickupOwner.h"

#include "SpeedBoostItemActor.generated.h"

class USpeedBoostItemComponent;

UCLASS()
class SANDBOX_API ASpeedBoostItemActor
    : public AActor
    , public IPickupOwner {
    GENERATED_BODY()
  public:
    ASpeedBoostItemActor();

    // IPickupOwner implementation
    virtual UPrimitiveComponent* get_pickup_collision_component() override;
    virtual bool should_destroy_after_pickup() const override;
    virtual float get_pickup_cooldown() const override;
    virtual void on_pre_pickup_effect(AActor* collector) override;
    virtual void on_post_pickup_effect(AActor* collector) override;
  protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
    UStaticMeshComponent* mesh_component{};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
    USpeedBoostItemComponent* speed_boost_component{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup")
    bool destroy_after_pickup{true};

    UPROPERTY(EditAnywhere,
              BlueprintReadWrite,
              Category = "Pickup",
              meta = (EditCondition = "!destroy_after_pickup"))
    float pickup_cooldown{5.0f};
};
