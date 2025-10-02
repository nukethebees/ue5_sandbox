#pragma once

#include <limits>

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
    virtual void BeginPlay() override;
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
    FVector location_a;
    FVector location_b;
    FVector current_direction;
    FVector ab_direction;
    FVector ba_direction;
    float pause_timer{std::numeric_limits<float>::infinity()};

    auto get_ab_direction() const { return (location_b - location_a).GetSafeNormal(); }
    auto get_ba_direction() const { return (location_a - location_b).GetSafeNormal(); }
    auto going_to_b() const { return current_direction == ab_direction; }
};
