#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActivatableComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UActivatableComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UActivatableComponent();
  protected:
    virtual void BeginPlay() override;
};
