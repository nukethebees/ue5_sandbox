#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DynamicPivotCameraComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UDynamicPivotCameraComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UDynamicPivotCameraComponent();
  protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, Category = "Rotation")
    float rotation_speed_deg_per_sec{10.0f};
  public:
    virtual void TickComponent(float DeltaTime,
                               ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;
  private:
    FVector pivot_point{};
    FVector initial_offset{};
    float current_angle_deg{0.0f};

    void update_camera_position(float delta_time);
};
