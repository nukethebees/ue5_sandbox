#include "Sandbox/subsystems/world/DamageManagerSubsystem.h"

void UDamageManagerSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UDamageManagerSubsystem::Initialize"))
    Super::Initialize(collection);

    (void)damage_queue.logged_init(n_queue_elements, "DamageManagerSubsystem::DamageQueue");
}

void UDamageManagerSubsystem::queue_health_change(UHealthComponent* receiver,
                                                  FHealthChange const& change) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UDamageManagerSubsystem::queue_health_change"))
    if (!receiver) {
        return;
    }

    (void)damage_queue.enqueue(FQueuedHealthChange{receiver, change});
    tick_enabled = true;
}

void UDamageManagerSubsystem::Tick(float DeltaTime) {
    TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Sandbox::UDamageManagerSubsystem::Tick"))
    Super::Tick(DeltaTime);

    auto changes{damage_queue.swap_and_consume()};
    changes.log_results(TEXT("UDamageManagerSubsystem::Tick"));

    for (auto const& change : changes.view) {
        if (change.Receiver.IsValid()) {
            change.Receiver->modify_health(change.Change);
        }
    }

    if (changes.view.empty()) {
        tick_enabled = false;
    }
}

TStatId UDamageManagerSubsystem::GetStatId() const {
    RETURN_QUICK_DECLARE_CYCLE_STAT(UDamageManagerSubsystem, STATGROUP_Tickables);
}
