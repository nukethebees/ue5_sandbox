#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RotatingCameraComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API URotatingCameraComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    URotatingCameraComponent();
  protected:
    virtual void BeginPlay() override;

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
    FVector pivot_point{};
    float current_angle_deg{0.0f};

    void update_camera_position(float delta_time);
};
