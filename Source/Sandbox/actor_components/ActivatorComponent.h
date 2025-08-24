#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActivatorComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UActivatorComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UActivatorComponent();
  protected:
    virtual void BeginPlay() override;
};
