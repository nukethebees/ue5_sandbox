#pragma once

#include <GameFramework/Actor.h>
#include <UObject/ObjectMacros.h>
#include <UObject/ObjectPtr.h>

#include "WarpVolume.generated.h"

class UBoxComponent;

UCLASS()
class AWarpVolume : public AActor {
    GENERATED_BODY()
  public:
    AWarpVolume();

    auto get_box() const -> UBoxComponent*;
  protected:
    UPROPERTY(EditAnywhere, Category = "Warp")
    TObjectPtr<UBoxComponent> box{nullptr};

};
