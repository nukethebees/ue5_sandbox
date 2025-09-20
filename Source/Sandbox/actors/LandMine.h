#pragma once

#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sandbox/data/LandMinePayload.h"
#include "Sandbox/interfaces/CollisionOwner.h"
#include "Sandbox/mixins/log_msg_mixin.hpp"

#include "LandMine.generated.h"

UENUM(BlueprintType)
enum class ELandMineState : uint8 {
    Active UMETA(DisplayName = "Active"),
    Warning UMETA(DisplayName = "Warning"),
    Detonating UMETA(DisplayName = "Detonating"),
    Deactivated UMETA(DisplayName = "Deactivated")
};

USTRUCT(BlueprintType)
struct FLandMineColours {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    FLinearColor active{FLinearColor::Green};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    FLinearColor warning{FLinearColor::Yellow};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    FLinearColor detonating{FLinearColor::Red};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    FLinearColor deactivated{FLinearColor::Blue};
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
    virtual UPrimitiveComponent* get_collision_component() override {
        return trigger_collision_component;
    }
    virtual bool should_destroy_after_collision() const override { return false; }
    virtual void on_pre_collision_effect(AActor& other_actor) override;

#if WITH_EDITOR
    virtual void OnConstruction(const FTransform& Transform) override;
#endif
  protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mine")
    UCapsuleComponent* warning_collision_component{};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mine")
    UCapsuleComponent* trigger_collision_component{};

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    ELandMineState current_state{ELandMineState::Active};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    float detonation_delay{2.0f};
  public:
    void change_state(ELandMineState new_state);

    UFUNCTION()
    void on_warning_enter(UPrimitiveComponent* OverlappedComponent,
                          AActor* OtherActor,
                          UPrimitiveComponent* OtherComp,
                          int32 OtherBodyIndex,
                          bool bFromSweep,
                          FHitResult const& SweepResult);

    UFUNCTION()
    void on_warning_exit(UPrimitiveComponent* OverlappedComponent,
                         AActor* OtherActor,
                         UPrimitiveComponent* OtherComp,
                         int32 OtherBodyIndex);
  private:
    void update_debug_sphere();
};
