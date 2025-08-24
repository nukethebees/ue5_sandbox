#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractorComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UInteractorComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UInteractorComponent();
  protected:
    virtual void BeginPlay() override;
};
