#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sandbox/environment/interactive/interfaces/Clickable.h"

#include "SlidingPlatformActorComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API USlidingPlatformActorComponent
    : public UActorComponent
    , public IClickable {
    GENERATED_BODY()
  public:
    USlidingPlatformActorComponent();
  protected:
    virtual void BeginPlay() override;
    virtual void OnClicked() override;
  public:
    virtual void TickComponent(float DeltaTime,
                               ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float move_speed{300.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float acceleration{100.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float move_distance{500.0f};
  private:
    bool moving_forward{true};
    FVector start_location;
    FVector end_location;
};
