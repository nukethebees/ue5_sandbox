#pragma once

#include <ioj/math_types.h>

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"

#include "StaticOrbitCameraComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UStaticOrbitCameraComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UStaticOrbitCameraComponent();
  protected:
    void BeginPlay() override;

    UPROPERTY(EditAnywhere, Category = "Rotation")
    float rotation_speed_deg_per_sec{10.0f};
    UPROPERTY(EditAnywhere, Category = "Rotation")
    float radius{500.0f};
    UPROPERTY(EditAnywhere, Category = "Rotation")
    float height{500.0f};
  public:
    virtual void TickComponent(float DeltaTime,
                               ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;
  private:
    ioj::Vector3d pivot_point_{};
    double current_angle_deg_{};

    void update_camera_position(float delta_time);
};
