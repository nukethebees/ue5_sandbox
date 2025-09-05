#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthStationComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UHealthStationComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UHealthStationComponent();
  protected:
    virtual void BeginPlay() override;
};
