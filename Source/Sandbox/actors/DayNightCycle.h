#pragma once

#include "Components/DirectionalLightComponent.h"
#include "CoreMinimal.h"
#include "Engine/DirectionalLight.h"
#include "GameFramework/Actor.h"

#include "DayNightCycle.generated.h"

UCLASS()
class SANDBOX_API ADayNightCycle : public AActor {
    GENERATED_BODY()
  public:
    ADayNightCycle();
  public:
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(EditAnywhere)
    TObjectPtr<ADirectionalLight> sun_light;
    UPROPERTY(EditAnywhere, Category = "Movement")
    float rotation_speed_degrees_per_second{1.0f};
};
