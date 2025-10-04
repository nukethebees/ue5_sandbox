#include "Sandbox/subsystems/world/BulletSparkEffectSubsystem.h"

#include "Sandbox/actors/BulletSparkEffectManagerActor.h"

void UBulletSparkEffectSubsystem::add_impact(FVector const& location, FRotator const& rotation) {}

void UBulletSparkEffectSubsystem::register_actor(ABulletSparkEffectManagerActor* actor) {
    manager_actor = actor;
}

void UBulletSparkEffectSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);
    constexpr auto logger{NestedLogger<"Initialize">()};

    constexpr std::size_t n_queue_elements{100};
    switch (queue.init(n_queue_elements)) {
        using enum ml::ELockFreeMPSCQueueInitResult;
        case Success: {
            break;
        }
        case AlreadyInitialised: {
            logger.log_error(TEXT("Queue initialised twice."));
            break;
        }
        default: {
            logger.log_error(TEXT("Unhandled ELockFreeMPSCQueueInitResult state."));
            break;
        }
    }
}
void UBulletSparkEffectSubsystem::Tick(float DeltaTime) {}

TStatId UBulletSparkEffectSubsystem::GetStatId() const {
    RETURN_QUICK_DECLARE_CYCLE_STAT(UBulletSparkEffectSubsystem, STATGROUP_Tickables);
}
