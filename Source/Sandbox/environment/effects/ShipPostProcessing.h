#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ShipPostProcessing.generated.h"

class UPostProcessComponent;

UCLASS()
class SANDBOX_API AShipPostProcessing : public AActor {
    GENERATED_BODY()
  public:
    AShipPostProcessing();
  protected:
    void OnConstruction(FTransform const& transform) override;

    void set_up_post_processing(UPostProcessComponent& pp);

    UPROPERTY(EditAnywhere, Category = "Post process")
    UPostProcessComponent* post_process_volume{nullptr};
};
