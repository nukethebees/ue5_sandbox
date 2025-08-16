#pragma once

#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sandbox/interfaces/clickable.h"

#include "MoveToTargetActorComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UMoveToTargetActorComponent
    : public UActorComponent
    , public IClickable {
    GENERATED_BODY()
  public:
    UMoveToTargetActorComponent();
  protected:
    virtual void BeginPlay() override;
    virtual void OnClicked() override;
  public:
    virtual void TickComponent(float DeltaTime,
                               ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float move_speed{300.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float acceleration{100.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float move_distance{500.0f};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    USceneComponent* target_component{nullptr};
  private:
    bool moving_forward{true};
    FVector start_location;
    FVector end_location;
};
