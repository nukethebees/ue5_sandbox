#pragma once

#include "CoreMinimal.h"
#include "Sandbox/actor_components/PickupComponent.h"
#include "Sandbox/data/SpeedBoost.h"

#include "SpeedBoostItemComponent.generated.h"

UCLASS(meta = (BlueprintSpawnableComponent))
class SANDBOX_API USpeedBoostItemComponent : public UPickupComponent {
    GENERATED_BODY()
  public:
    USpeedBoostItemComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    FSpeedBoost speed_boost{};
  protected:
    virtual void execute_pickup_effect(AActor* target_actor) override;
};
