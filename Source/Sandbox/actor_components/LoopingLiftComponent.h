#pragma once

#include <limits>

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"

#include "LoopingLiftComponent.generated.h"

// A lift that moves from its origin by distance X and then back again
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API ULoopingLiftComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    ULoopingLiftComponent();
  protected:
    virtual void BeginPlay() override;
  public:
    virtual void TickComponent(float DeltaTime,
                               ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float speed{200.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float distance{200.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float pause_after_completion{0.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    FVector direction{FVector::UpVector};
  private:
    FVector origin;
    FVector current_direction;
    FVector original_direction;
    float pause_timer{std::numeric_limits<float>::infinity()};
};
