#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "Sandbox/environment/effects/enums/RotationType.h"

#include "RotatingActorComponent.generated.h"

class USceneComponent;
class UWorld;
class URotationManagerSubsystem;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API URotatingActorComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    URotatingActorComponent();
  protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
  public:
    void register_scene_component(URotationManagerSubsystem& rotation_manager,
                                  UWorld& world,
                                  USceneComponent& scene_component);
    void register_scene_component(USceneComponent& scene_component);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    FRotator rotation_speed{0.f, 50.f, 0.f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    ERotationType rotation_type{ERotationType::STATIC};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    bool destroy_component_after_static_registration{true};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    bool autoregster_parent_root{true};
  private:
    void unregister_from_subsystem();
};
