#pragma once

#include "CoreMinimal.h"
#include "Sandbox/health/HealthChange.h"
#include "Sandbox/health/StationStateData.h"
#include "Sandbox/interaction/TriggerContext.h"
#include "Sandbox/interaction/TriggerResult.h"
#include "Sandbox/logging/LogMsgMixin.hpp"

#include "HealthStationPayload.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnStationStateChanged, FStationStateData const&);

USTRUCT(BlueprintType)
struct FHealthStationPayload {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Station")
    float heal_amount_per_use{25.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Station")
    float max_capacity{100.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Station")
    float cooldown_duration{2.0f};

    // Runtime state
    float current_capacity{0.0f};
    float cooldown_remaining{0.0f};

    FOnStationStateChanged on_station_state_changed;

    // Static logger since can't inherit from LogMsgMixin
    static inline auto logger{ml::LogMsgMixin<"FHealthStationPayload">{}};

    FTriggerResult trigger(FTriggerContext context);
    bool tick(float delta_time);
    bool can_trigger(FTriggerContext const& context) const;
    void reset_capacity() { current_capacity = max_capacity; }
    bool is_ready() const { return current_capacity > 0.0f && cooldown_remaining <= 0.0f; }
    void broadcast_state() const;
};
