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

    FTriggerContext() = delete;
    FTriggerContext(UWorld& world,
                    AActor& triggered_actor,
                    FTriggeringSource source,
                    float trigger_strength,
                    FVector trigger_location,
                    float delta_time)
        : world(world)
        , triggered_actor(triggered_actor)
        , source(source)
        , trigger_strength(trigger_strength)
        , trigger_location(trigger_location)
        , delta_time(delta_time) {}
};
