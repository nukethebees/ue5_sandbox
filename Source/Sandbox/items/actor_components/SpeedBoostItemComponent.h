#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sandbox/items/data/SpeedBoost.h"

#include "SpeedBoostItemComponent.generated.h"

UCLASS(meta = (BlueprintSpawnableComponent))
class SANDBOX_API USpeedBoostItemComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    USpeedBoostItemComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    FSpeedBoost speed_boost{};
  protected:
    virtual void BeginPlay() override;
};
