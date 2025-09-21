#pragma once

#include <optional>

#include "CoreMinimal.h"
#include "Sandbox/data/trigger/TriggerableId.h"
#include "Sandbox/data/trigger/TriggerCapabilities.h"

class AActor;
class UWorld;

struct FTriggeringSource {
    ETriggerForm type{ETriggerForm::Unknown};
    FTriggerCapabilities capabilities;
    AActor* instigator{nullptr};
    FVector source_location{};
    std::optional<TriggerableId> source_triggerable{};

    bool is_from_triggerable() const { return source_triggerable.has_value(); }
    bool has_capability(ETriggerCapability cap) const { return capabilities.has_capability(cap); }
    AActor* get_instigator() const { return instigator; }
    TriggerableId get_source_id() const {
        return source_triggerable.value_or(TriggerableId::invalid());
    }
};

struct FTriggerContext {
    UWorld& world;
    AActor& triggered_actor;
    FTriggeringSource source;
    float trigger_strength{1.0f};
    FVector trigger_location{};
    float delta_time{};
};
