#pragma once

#include <CoreMinimal.h>
#include <GameFramework/Actor.h>

#include "TestCollisionActor.generated.h"

class UBoxComponent;

UCLASS()
class ASandboxTestCollisionActor : public AActor {
    GENERATED_BODY()
  public:
    ASandboxTestCollisionActor();

    auto get_collision_component() const noexcept -> UBoxComponent* { return collision_component; }
  private:
    UPROPERTY()
    TObjectPtr<UBoxComponent> collision_component;
};

UCLASS()
class ASandboxTestDerivedCollisionActor : public ASandboxTestCollisionActor {
    GENERATED_BODY()
};

UCLASS()
class ASandboxTestOmittedCollisionActor : public AActor {
    GENERATED_BODY()
  public:
    ASandboxTestOmittedCollisionActor();

    auto get_collision_component() const noexcept -> UBoxComponent* { return collision_component; }
  private:
    UPROPERTY()
    TObjectPtr<UBoxComponent> collision_component;
};
