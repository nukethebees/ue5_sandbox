#pragma once

#include "Sandbox/health/HealthChange.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ShipBomb.generated.h"

class UStaticMeshComponent;
class UNiagaraSystem;
class UBoxComponent;

struct FShipDamageResult;

UCLASS()
class SANDBOX_API AShipBomb : public AActor {
    GENERATED_BODY()
  public:
    AShipBomb();

    void Tick(float dt) override;
    void detonate();
    bool has_detonated() const { return detonated; }
    void set_target(AActor* tgt) { target = tgt; }
  protected:
    void BeginPlay() override;

    bool explosion_complete() const { return time_since_detonation >= explosion_lifetime; }

#if WITH_EDITOR
    void draw_debug_sphere();
#endif

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bomb")
    UStaticMeshComponent* mesh_component{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bomb")
    UBoxComponent* collision_box{nullptr};

    UPROPERTY(EditAnywhere, Category = "Bomb")
    UNiagaraSystem* explosion_effect{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bomb")
    AActor* target{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bomb")
    float speed{10000.f};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bomb")
    float fuse_time{2.f};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bomb")
    float lock_on_fuse_time{10.f};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bomb")
    float explosion_radius{500.f};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bomb")
    float explosion_lifetime{3.f};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bomb")
    int32 damage{50};

    UPROPERTY(VisibleAnywhere, Category = "Bomb")
    bool detonated{false};
    TSet<AActor const*> actors_hit;
    UPROPERTY(VisibleAnywhere, Category = "Bomb")
    float time_since_detonation{0.f};

#if WITH_EDITORONLY_DATA
    bool debug_explosion{true};
#endif
};
