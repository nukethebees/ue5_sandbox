#pragma once

#include "Sandbox/interaction/CollisionOwner.h"
#include "Sandbox/interaction/CollisionPayloads.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Coin.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;

UCLASS()
class SANDBOX_API ACoin
    : public AActor
    , public ICollisionOwner {
    GENERATED_BODY()
  public:
    ACoin();

    virtual UPrimitiveComponent* get_collision_component() override;
    virtual bool should_destroy_after_collision() const;
  protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Coin")
    UStaticMeshComponent* mesh_component{};
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Coin")
    UBoxComponent* collision_component;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coin")
    FRotator rotation_speed{0.f, 100.0f, 0.f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coin")
    int32 coin_value{1};
};
