#pragma once

#include "Sandbox/environment/SandboxActorSpawner.h"
#include "Sandbox/misc/learning/TestFlySeekDestroyEvade.h"

#include "TestFlySeekDestroyEvadeSpawner.generated.h"

UCLASS()
class ATestFlySeekDestroyEvadeSpawner : public ASandboxActorSpawner {
  public:
    GENERATED_BODY()

    ATestFlySeekDestroyEvadeSpawner() = default;
  protected:
    void configure_instance(AActor& instance) override;

    UPROPERTY(EditAnywhere, Category = "Fly")
    FTestFlySeekDestroyEvadeConfig config;
};
