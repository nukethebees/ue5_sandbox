#pragma once

#include "Sandbox/items/SpeedBoost.h"

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

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
