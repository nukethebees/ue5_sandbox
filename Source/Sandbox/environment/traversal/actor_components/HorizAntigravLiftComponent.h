#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HorizAntigravLiftComponent.generated.h"

class UBoxComponent;
class AMyCharacter;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UHorizAntigravLiftComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UHorizAntigravLiftComponent();
  protected:
    virtual void BeginPlay() override;
  public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Float")
    UBoxComponent* float_zone{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Float")
    float float_speed{150.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Float")
    FVector float_direction = FVector{1.0f, 0.0f, 0.0f};

    virtual void TickComponent(float DeltaTime,
                               ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp,
                        AActor* other_actor,
                        UPrimitiveComponent* OtherComp,
                        int32 other_body_index,
                        bool from_sweep,
                        FHitResult const& sweep_result);

    UFUNCTION()
    void OnOverlapEnd(UPrimitiveComponent* OverlappedComp,
                      AActor* other_actor,
                      UPrimitiveComponent* OtherComp,
                      int32 other_body_index);
  private:
    TSet<AMyCharacter*> floating_characters;
};
