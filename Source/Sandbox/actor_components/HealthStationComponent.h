#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Sandbox/interfaces/Interactable.h"

#include "HealthStationComponent.generated.h"

USTRUCT(BlueprintType)
struct FStationStateData {
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    float remaining_capacity;

    UPROPERTY(BlueprintReadOnly)
    float cooldown_remaining;

    UPROPERTY(BlueprintReadOnly)
    bool is_ready;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStationStateChanged, FStationStateData, state_data);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SANDBOX_API UHealthStationComponent
    : public UActorComponent
    , public IInteractable {
    GENERATED_BODY()
  public:
    UHealthStationComponent();

    virtual void interact(AActor* interactor = nullptr) override;
    virtual bool can_interact(AActor const* interactor) const override;
  protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime,
                               ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category = "Health Station")
    float get_current_capacity() const { return current_capacity; }

    UPROPERTY(EditAnywhere, Category = "Health Station")
    float max_capacity{100.0f};

    UPROPERTY(EditAnywhere, Category = "Health Station")
    float heal_amount_per_use{25.0f};

    UPROPERTY(EditAnywhere, Category = "Health Station")
    float cooldown_duration{2.0f}; // seconds

    UPROPERTY(BlueprintAssignable, Category = "Health Station")
    FOnStationStateChanged on_station_state_changed;
  private:
    void reset_current_capacity();
    void broadcast_state() const;
    void start_cooldown();

    float current_capacity{0.0f};
    float cooldown_remaining{0.0f};
    FTimerHandle cooldown_timer_handle;
};
