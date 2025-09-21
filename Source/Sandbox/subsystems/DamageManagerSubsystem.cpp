#include "Sandbox/subsystems/DamageManagerSubsystem.h"

void UDamageManagerSubsystem::queue_health_change(UHealthComponent* receiver,
                                           FHealthChange const& change) {
    if (!receiver) {
        return;
    }

    pending_changes.Add({receiver, change});
    tick_enabled = true;
}
void UDamageManagerSubsystem::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);

    if (pending_changes.IsEmpty()) {
        tick_enabled = false;
        return;
    }

    for (auto const& change : pending_changes) {
        if (change.Receiver.IsValid()) {
            change.Receiver->modify_health(change.Change);
        }
    }

    pending_changes.Empty();
    tick_enabled = false;
}
TStatId UDamageManagerSubsystem::GetStatId() const {
    RETURN_QUICK_DECLARE_CYCLE_STAT(UDamageManagerSubsystem, STATGROUP_Tickables);
}
