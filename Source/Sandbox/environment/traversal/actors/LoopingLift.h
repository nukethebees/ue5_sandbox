#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "LoopingLift.generated.h"

UCLASS()
class SANDBOX_API ALoopingLift : public AActor {
    GENERATED_BODY()
  public:
    ALoopingLift();
  protected:
    virtual void BeginPlay() override;
  public:
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float speed{200.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float distance{200.0f};
  private:
    FVector origin;
    bool going_up;
};
