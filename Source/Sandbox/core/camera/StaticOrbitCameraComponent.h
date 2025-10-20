#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StaticOrbitCameraComponent.generated.h"

// Make a camera actor rotate around a point
// The point is either set with an arrow component or via a raycast from the camera
// to the first object hit

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UStaticOrbitCameraComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UStaticOrbitCameraComponent();
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
