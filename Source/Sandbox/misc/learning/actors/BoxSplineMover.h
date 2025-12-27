#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "BoxSplineMover.generated.h"

class USplineComponent;
class UStaticMeshComponent;

UCLASS()
class SANDBOX_API ABoxSplineMover : public AActor {
    GENERATED_BODY()
  public:
    ABoxSplineMover();
  protected:
    virtual void BeginPlay() override;
  public:
    virtual void Tick(float DeltaTime) override;
  protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USplineComponent* spline_component{};

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* mesh_component{};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float movement_speed{100.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    bool loop_movement{true};
  private:
    float current_distance{0.0f};
    float spline_length{0.0f};
    bool moving_forward{true};

    void update_position_along_spline(float distance);
};
