#pragma once

#include "SandboxGameShared/interaction/Clickable.h"
#include <ioj/linear_travel.h>

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"

#include "SlidingPlatformActorComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API USlidingPlatformActorComponent
    : public UActorComponent
    , public IClickable {
    GENERATED_BODY()
  public:
    USlidingPlatformActorComponent();
  protected:
    void BeginPlay() override;
    void OnClicked() override;
  public:
    virtual void TickComponent(float DeltaTime,
                               ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float move_speed{300.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float move_distance{500.0f};
  private:
    ioj::LinearTravelState travel_{};
};
