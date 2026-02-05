#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "LaunchpadComponent.generated.h"

class UBoxComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API ULaunchpadComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    ULaunchpadComponent();
  protected:
    virtual void BeginPlay() override;
  private:
    UFUNCTION()
    void on_overlap_begin(UPrimitiveComponent* overlapped_comp,
                          AActor* other_actor,
                          UPrimitiveComponent* other_comp,
                          int32 other_body_index,
                          bool b_from_sweep,
                          FHitResult const& sweep_result);
  public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Launch")
    UBoxComponent* collision_box{nullptr};

    UPROPERTY(EditAnywhere, Category = "Launch")
    float launch_strength{1000.0f};
};
