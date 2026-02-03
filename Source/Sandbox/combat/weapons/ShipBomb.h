#pragma once

#include "Sandbox/health/HealthChange.h"

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/Actor.h"

#include "ShipBomb.generated.h"

class UStaticMeshComponent;

UCLASS()
class SANDBOX_API AShipBomb : public AActor {
    GENERATED_BODY()
  public:
    AShipBomb();

    void Tick(float dt) override;
    void detonate();
  protected:
    void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bomb")
    UStaticMeshComponent* mesh_component{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bomb")
    float speed{10000.f};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bomb")
    float fuse_time{2.f};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bomb")
    float explosion_radius{500.f};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bomb")
    FHealthChange damage{50.f, EHealthChangeType::Damage};

    FTimerHandle timer;
};
