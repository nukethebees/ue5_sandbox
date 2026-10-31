#pragma once

#include <ioj/linear_travel.h>

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"

#include "LoopingPlatformComponent.generated.h"

class USceneComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API ULoopingPlatformComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    ULoopingPlatformComponent();
  protected:
    void BeginPlay() override;
  public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float speed{200.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float pause_after_completion{0.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    USceneComponent* target_component{nullptr};
    virtual void TickComponent(float DeltaTime,
                               ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;
  private:
    ioj::LinearTravelState travel_{};
};
