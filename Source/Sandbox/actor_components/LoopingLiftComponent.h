#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "LoopingLiftComponent.generated.h"

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
  private:
    FVector origin;
    bool going_up;
};
