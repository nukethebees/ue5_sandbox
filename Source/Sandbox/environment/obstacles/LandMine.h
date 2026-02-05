#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Sandbox/environment/obstacles/LandMinePayload.h"
#include "Sandbox/environment/obstacles/LandMineState.h"
#include "Sandbox/health/DeathHandler.h"
#include "Sandbox/interaction/CollisionOwner.h"
#include "Sandbox/interaction/Describable.h"
#include "Sandbox/logging/LogMsgMixin.hpp"
#include "Sandbox/logging/SandboxLogCategories.h"

#include "LandMine.generated.h"

class UCapsuleComponent;
class UPointLightComponent;
class USphereComponent;
class UStaticMeshComponent;

class UHealthComponent;

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
    , public IDeathHandler
    , public ml::LogMsgMixin<"ALandMine", LogSandboxActor>
    , public IDescribable {
    GENERATED_BODY()
  public:
    ALandMine();

    // ICollisionOwner implementation
    virtual UPrimitiveComponent* get_collision_component() override;
    virtual bool should_destroy_after_collision() const override { return true; }
    virtual float get_destruction_delay() const override;
    virtual void on_pre_collision_effect(AActor& other_actor) override;

    // IDescribable
    virtual FText get_description() const override {
        static auto const desc{FText::FromName(TEXT("Mine"))};
        return desc;
    }
  protected:
    virtual void OnConstruction(FTransform const& Transform) override;
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    UCapsuleComponent* warning_collision_component{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    UCapsuleComponent* trigger_collision_component{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    UStaticMeshComponent* mesh_component{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    UPointLightComponent* light_component{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    UHealthComponent* health_component{};

#if WITH_EDITORONLY_DATA
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mine")
    USphereComponent* explosion_radius_debug{};
#endif

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    FLandMinePayload payload_config{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    FLandMineColours colours;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    ELandMineState current_state{ELandMineState::Active};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    float collision_half_height{15.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    float warning_radius{150.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mine")
    float trigger_radius{50.0f};
  public:
    void change_state(ELandMineState new_state);

    UFUNCTION()
    void on_warning_enter(UPrimitiveComponent* overlapped_component,
                          AActor* other_actor,
                          UPrimitiveComponent* OtherComp,
                          int32 other_body_index,
                          bool from_sweep,
                          FHitResult const& sweep_result);

    UFUNCTION()
    void on_warning_exit(UPrimitiveComponent* overlapped_component,
                         AActor* other_actor,
                         UPrimitiveComponent* OtherComp,
                         int32 other_body_index);

    // IDeathHandler
    virtual void handle_death() override;
  private:
    void update_debug_sphere();
    void update_trigger_sizes();
};
