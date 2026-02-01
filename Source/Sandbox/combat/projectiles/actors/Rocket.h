#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Rocket.generated.h"

class UStaticMeshComponent;
class UProjectileMovementComponent;
class UBoxComponent;

struct FHitResult;

UCLASS()
class SANDBOX_API ARocket : public AActor {
    GENERATED_BODY()
  public:
    ARocket();

    void Tick(float dt) override;

    void fire(float speed);
  protected:
    void BeginPlay() override;

    UFUNCTION()
    void on_hit(UPrimitiveComponent* HitComponent,
                AActor* other_actor,
                UPrimitiveComponent* other_component,
                FVector NormalImpulse,
                FHitResult const& Hit);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
    UProjectileMovementComponent* projectile_movement{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    UBoxComponent* collision_box{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    UStaticMeshComponent* mesh_component{nullptr};

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    bool fire_on_launch{false};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
    float speed{100.f};
#endif
};
