#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Sandbox/interaction/collision/data/CollisionPayloads.h"
#include "Sandbox/interaction/collision/interfaces/CollisionOwner.h"

#include "Coin.generated.h"

UCLASS()
class SANDBOX_API ACoin
    : public AActor
    , public ICollisionOwner {
    GENERATED_BODY()
  public:
    ACoin();

    virtual UPrimitiveComponent* get_collision_component() override { return collision_component; }
    virtual bool should_destroy_after_collision() const override { return true; }
  protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Coin")
    UStaticMeshComponent* mesh_component{};
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Coin")
    UBoxComponent* collision_component;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coin")
    float rotation_speed{100.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coin")
    int32 coin_value{1};
};
