#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Sandbox/data/HealthStationPayload.h"
#include "Sandbox/data/TriggerableId.h"

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
class SANDBOX_API UHealthStationComponent : public UActorComponent {
    GENERATED_BODY()
  public:
    UHealthStationComponent();

    void broadcast_state() const;

    UPROPERTY(BlueprintAssignable, Category = "Health Station")
    FOnStationStateChanged on_station_state_changed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Station")
    FHealthStationPayload health_station_payload;

    UFUNCTION(BlueprintCallable, Category = "Health Station")
    float get_current_capacity() const { return health_station_payload.current_capacity; }

    UFUNCTION(BlueprintCallable, Category = "Health Station")
    bool is_ready() const { return health_station_payload.is_ready(); }
  protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type reason) override;
  private:
    TriggerableId my_trigger_id{};
};
