#include "Sandbox/subsystems/world/BulletSparkEffectSubsystem.h"

#include "Delegates/DelegateInstancesImpl.h"

#include "Sandbox/actors/BulletSparkEffectManagerActor.h"

#include "Sandbox/macros/null_checks.hpp"

void UBulletSparkEffectSubsystem::add_impact(FVector const& location, FRotator const& rotation) {
    constexpr auto logger{NestedLogger<"add_impact">()};

    logger.log_verbose(TEXT("Adding impact to %s"), *location.ToString());

    switch (queue.enqueue(location, rotation)) {
        using enum ml::ELockFreeMPSCQueueEnqueueResult;
        case Success: {
            break;
        }
        case Full: {
            logger.log_warning(TEXT("Queue is full."));
            return;
        }
        case Uninitialised: {
            logger.log_error(TEXT("Queue is uninitialised."));
            return;
        }
        default: {
            break;
        }
    }
}

void UBulletSparkEffectSubsystem::register_actor(ABulletSparkEffectManagerActor* actor) {
    constexpr auto logger{NestedLogger<"register_actor">()};

    if (manager_actor.IsValid()) {
        logger.log_error(TEXT("Trying to register another ABulletSparkEffectManagerActor actor."));
        return;
    }

    manager_actor = actor;
}
void UBulletSparkEffectSubsystem::unregister_actor() {
    manager_actor = nullptr;
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

void UBulletSparkEffectSubsystem::Deinitialize() {
    FCoreDelegates::OnEndFrame.RemoveAll(this);
    unregister_actor();
    Super::Deinitialize();
}
void UBulletSparkEffectSubsystem::OnWorldBeginPlay(UWorld& world) {
    Super::OnWorldBeginPlay(world);
    FCoreDelegates::OnEndFrame.AddUObject(this, &UBulletSparkEffectSubsystem::on_end_frame);
}

void UBulletSparkEffectSubsystem::Tick(float DeltaTime) {}

void UBulletSparkEffectSubsystem::on_end_frame() {
    RETURN_IF_INVALID(manager_actor);

    if (auto* world{GetWorld()}) {
        if (!world->IsGameWorld()) {
            return;
        }
        if (world->IsPaused()) {
            return;
        }
    }

    queue.swap_and_visit([this](std::span<FSparkEffectTransform> impacts) {
        if (impacts.empty()) {
            return;
        }

        manager_actor->consume_impacts(impacts);
    });
}

TStatId UBulletSparkEffectSubsystem::GetStatId() const {
    RETURN_QUICK_DECLARE_CYCLE_STAT(UBulletSparkEffectSubsystem, STATGROUP_Tickables);
}
