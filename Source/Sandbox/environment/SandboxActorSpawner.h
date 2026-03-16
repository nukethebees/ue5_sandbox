#pragma once

#include "GameFramework/Actor.h"

#include <limits>

#include "SandboxActorSpawner.generated.h"

class UStaticMeshComponent;
class UArrowComponent;

UCLASS()
class ASandboxActorSpawner : public AActor {
    GENERATED_BODY()
  public:
    ASandboxActorSpawner();

    void Tick(float dt) override;
  protected:
    void BeginPlay() override;
    void EndPlay(EEndPlayReason::Type const reason) override;

    void spawn();
    void destroy_all_actors();
    auto clear_destroyed_actors() -> int32;

    UPROPERTY(EditAnywhere, Category = "Spawner")
    UStaticMeshComponent* mesh{nullptr};
    UPROPERTY(EditAnywhere, Category = "Spawner")
    UArrowComponent* arrow{nullptr};
    UPROPERTY(EditAnywhere, Category = "Spawner")
    TSubclassOf<AActor> actor_class{};

    UPROPERTY(EditAnywhere, Category = "Spawner")
    int32 spawn_limit{std::numeric_limits<int32>::max()};
    UPROPERTY(EditAnywhere, Category = "Spawner")
    float spawn_period{1.f};
    UPROPERTY(EditAnywhere, Category = "Spawner")
    float actor_lifespan{0.f};

    UPROPERTY(VisibleAnywhere, Category = "Spawner")
    TArray<AActor*> spawned_actors{};

    // Errors
    bool printed_null_actor_class_warning{false};
};
