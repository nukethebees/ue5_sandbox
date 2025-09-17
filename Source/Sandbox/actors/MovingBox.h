#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MovingBox.generated.h"

UCLASS()
class SANDBOX_API AMovingBox : public AActor {
    GENERATED_BODY()
  public:
    AMovingBox();
  protected:
    virtual void BeginPlay() override;
  public:
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    float move_speed{100.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement");
    float move_distance{1000.0};
};
