#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "AntigravLiftActorComponent.generated.h"

class UBoxComponent;
class ACharacter;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UAntigravLiftActorComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UAntigravLiftActorComponent();
  protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime,
                               ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lift")
    UBoxComponent* lift_zone{nullptr};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lift")
    float lift_speed{250.0f};

    // Determines lift direction
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lift")
    bool is_antigravity{true};

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
    void display_n_floating();

    TSet<ACharacter*> floating_characters;
};
