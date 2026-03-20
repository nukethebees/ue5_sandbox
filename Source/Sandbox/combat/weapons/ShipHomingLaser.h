#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ShipHomingLaser.generated.h"

class UProjectileMovementComponent;
class UStaticMeshComponent;
class UBoxComponent;
class UNiagaraSystem;
class UWorld;

UCLASS()
class SANDBOX_API AShipHomingLaser : public AActor {
    GENERATED_BODY()
  public:
    AShipHomingLaser();

    void Tick(float dt) override;

    void set_speed(float s) { speed = s; }
    auto get_speed(float s) const { return speed; }
    void set_target(AActor* t) { target = t; }
    auto get_target(AActor* t) const { return target; }
  protected:
    void BeginPlay() override;
    void OnConstruction(FTransform const& transform);

    void explode();
    void apply_damage(UWorld& world);

#if WITH_EDITOR
    void draw_debug_sphere();
#endif

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Laser")
    UStaticMeshComponent* mesh_component{nullptr};
    UPROPERTY(EditAnywhere, Category = "Laser")
    UBoxComponent* collision_box{nullptr};

    UPROPERTY(EditAnywhere, Category = "Laser")
    UNiagaraSystem* explosion_effect{nullptr};

    UPROPERTY(EditAnywhere, Category = "Laser")
    AActor* target{nullptr};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser")
    int32 damage{10};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser")
    float speed{50000.f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Laser")
    float explosion_radius{100.f};

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool debug_explosion{true};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    float debug_explosion_lifetime{3.f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    float debug_line_thickness{30.f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    FColor debug_line_colour{FColor::Green};
#endif
};
