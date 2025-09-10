#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Sandbox/interfaces/Interactable.h"

#include "HealthStationComponent.generated.h"

USTRUCT(BlueprintType)
struct FStationStateData {
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    float remaining_capacity{0.0f};

    UPROPERTY(BlueprintReadOnly)
    float cooldown_remaining{0.0f};

    UPROPERTY(BlueprintReadOnly)
    float cooldown_total{1.0f};

    UPROPERTY(BlueprintReadOnly)
    bool is_ready{false};
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStationStateChanged,
                                            FStationStateData const&,
                                            state_data);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UHealthStationComponent
    : public UActorComponent
    , public IInteractable {
    GENERATED_BODY()
  public:
    UHealthStationComponent();

    virtual void interact(AActor* interactor = nullptr) override;
    virtual bool can_interact(AActor const* interactor) const override;

    void broadcast_state() const;

    UPROPERTY(BlueprintAssignable, Category = "Health Station")
    FOnStationStateChanged on_station_state_changed;
  protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime,
                               ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category = "Health Station")
    float get_current_capacity() const { return current_capacity; }

    UPROPERTY(EditAnywhere, Category = "Health Station")
    float max_capacity{100.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Station")
    float heal_amount_per_use{25.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Station")
    float cooldown_duration{2.0f}; // seconds
  private:
    void reset_current_capacity();
    void start_cooldown();

    float current_capacity{0.0f};
    float cooldown_remaining{0.0f};
    FTimerHandle cooldown_timer_handle;
};
