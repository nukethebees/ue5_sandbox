#include "Sandbox/health/DamageManagerSubsystem.h"

#include "Sandbox/health/HealthComponent.h"

#include "Sandbox/utilities/macros/null_checks.hpp"

void UDamageManagerSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UDamageManagerSubsystem::Initialize"))
    Super::Initialize(collection);

    (void)damage_queue.logged_init(n_queue_elements, "DamageManagerSubsystem::DamageQueue");
}

void UDamageManagerSubsystem::queue_health_change(UHealthComponent* receiver,
                                                  FHealthChange const& change) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UDamageManagerSubsystem::queue_health_change"))
    constexpr auto logger{NestedLogger<"queue_health_change">()};
    logger.log_very_verbose(TEXT("Queuing health change."));

    RETURN_IF_NULLPTR(receiver);

    (void)damage_queue.enqueue(FQueuedHealthChange{receiver, change});
    tick_enabled = true;
}

void UDamageManagerSubsystem::Tick(float delta_time) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UDamageManagerSubsystem::Tick"))
    constexpr auto logger{NestedLogger<"Tick">()};
    Super::Tick(delta_time);

    auto changes{damage_queue.swap_and_consume()};
    changes.log_results(TEXT("UDamageManagerSubsystem::Tick"));

    logger.log_verbose(TEXT("Processing %d damage events."), changes.view.size());
    for (auto const& change : changes.view) {
        CONTINUE_IF_FALSE(change.Receiver.IsValid())
        change.Receiver->modify_health(change.Change);
    }

    if (changes.view.empty()) {
        tick_enabled = false;
    }
}

TStatId UDamageManagerSubsystem::GetStatId() const {
    RETURN_QUICK_DECLARE_CYCLE_STAT(UDamageManagerSubsystem, STATGROUP_Tickables);
}
