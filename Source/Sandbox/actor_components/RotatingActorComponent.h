#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RotatingActorComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API URotatingActorComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    URotatingActorComponent();
  protected:
    virtual void BeginPlay() override;
  public:
    virtual void TickComponent(float DeltaTime,
                               ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float speed{50.0f};
};
