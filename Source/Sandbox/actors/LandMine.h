#pragma once

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sandbox/data/LandMinePayload.h"
#include "Sandbox/interfaces/CollisionOwner.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "LandMine.generated.h"

USTRUCT(BlueprintType)
struct FLandMineColours {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    FLinearColor active{FLinearColor::Red};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    FLinearColor disabled{FLinearColor::Blue};
};

UCLASS()
class SANDBOX_API ALandMine
    : public AActor
    , public ICollisionOwner
    , public ml::LogMsgMixin<"ALandMine"> {
    GENERATED_BODY()
  public:
    ALandMine();

    // ICollisionOwner implementation
    virtual UPrimitiveComponent* get_collision_component() override { return collision_component; }
    virtual bool should_destroy_after_collision() const override { return true; }
    virtual void on_pre_collision_effect(AActor& other_actor) override;

#if WITH_EDITOR
    virtual void OnConstruction(const FTransform& Transform) override;
#endif
  protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mine")
    UBoxComponent* collision_component{};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mine")
    UStaticMeshComponent* mesh_component{};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mine")
    UPointLightComponent* light_component{};

#if WITH_EDITORONLY_DATA
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mine")
    USphereComponent* explosion_radius_debug{};
#endif

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    float explosion_radius{100.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    float max_damage{25.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    float explosion_force{1000.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    FLandMineColours colours;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    class UParticleSystem* explosion_effect{};
  private:
    void update_debug_sphere();
};
