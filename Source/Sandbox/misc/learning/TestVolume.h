#pragma once

#include "Sandbox/utilities/BoxSize.h"

#include <GameFramework/Actor.h>
#include <UObject/ObjectMacros.h>
#include <UObject/ObjectPtr.h>

#include "TestVolume.generated.h"

class UBoxComponent;

UCLASS()
class ATestVolume : public AActor {
    GENERATED_BODY()
  public:
    ATestVolume();

    auto get_box() const -> UBoxComponent*;
  protected:
    UPROPERTY(EditAnywhere, Category = "Volume")
    TObjectPtr<UBoxComponent> box{nullptr};

    UPROPERTY(EditAnywhere, Category = "Volume")
    FBoxSize box_size;
};
