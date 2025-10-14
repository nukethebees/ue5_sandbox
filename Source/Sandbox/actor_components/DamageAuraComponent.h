#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "DamageAuraComponent.generated.h"

class UBoxComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UDamageAuraComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UDamageAuraComponent();
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

    UFUNCTION()
    void on_overlap_end(UPrimitiveComponent* overlapped_comp,
                        AActor* other_actor,
                        UPrimitiveComponent* other_comp,
                        int32 other_body_index);

    void apply_damage();
    void update_timer();
  public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Aura")
    float damage_per_second{10.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Aura")
    float tick_interval{0.1f};
  private:
    UPROPERTY()
    UBoxComponent* collision_box{nullptr};

    TSet<TWeakObjectPtr<AActor>> actors_in_aura{};
    FTimerHandle damage_timer{};
};
