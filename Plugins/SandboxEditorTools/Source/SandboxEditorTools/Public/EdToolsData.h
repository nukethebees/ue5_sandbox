#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "EdToolsData.generated.h"

UCLASS()
class UEdToolsData : public UObject {
    GENERATED_BODY()
  public:
    UPROPERTY(EditAnywhere)
    AActor* selected_actor{nullptr};
};
