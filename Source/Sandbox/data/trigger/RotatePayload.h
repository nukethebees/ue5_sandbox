#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Sandbox/data/trigger/TriggerContext.h"
#include "Sandbox/data/trigger/TriggerResult.h"
#include "Sandbox/mixins/LogMsgMixin.hpp"

#include "RotatePayload.generated.h"

USTRUCT(BlueprintType)
struct FRotatePayload {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
    FRotator rotation_rate{0.0f, 90.0f, 0.0f};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotation")
    float rotation_duration_seconds{2.0f};

    // Runtime state
    float time_remaining{0.0f};
    bool is_rotating{false};
    TWeakObjectPtr<AActor> rotating_actor{nullptr};

    // Static logger since can't inherit from LogMsgMixin
    static inline auto logger{ml::LogMsgMixin<"FRotatePayload">{}};

    FTriggerResult trigger(FTriggerContext context);
    bool tick(float delta_time);
    void start_rotation();
    void stop_rotation();
    bool is_active() const { return is_rotating && time_remaining > 0.0f; }
};
