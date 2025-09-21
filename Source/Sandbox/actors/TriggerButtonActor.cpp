#include "Sandbox/actors/TriggerButtonActor.h"

#include "Components/SceneComponent.h"
#include "Sandbox/subsystems/TriggerSubsystem.h"

ATriggerButtonActor::ATriggerButtonActor() {
    PrimaryActorTick.bCanEverTick = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ATriggerButtonActor::BeginPlay() {
    Super::BeginPlay();

    // Set the activation delay on the payload
    trigger_payload.activation_delay = activation_delay;

    // Convert target actors to TriggerableIds
    register_targets_with_payload();

    // Register with trigger subsystem
    auto* subsystem{GetWorld()->GetSubsystem<UTriggerSubsystem>()};
    if (subsystem) {
        my_trigger_id = subsystem->register_triggerable(this, std::move(trigger_payload));
    }
}

void ATriggerButtonActor::EndPlay(EEndPlayReason::Type reason) {
    if (auto* subsystem{GetWorld()->GetSubsystem<UTriggerSubsystem>()}) {
        subsystem->deregister_triggerable(this);
    }
    Super::EndPlay(reason);
}

void ATriggerButtonActor::add_target_actor(AActor* actor) {
    if (actor && !target_actors.Contains(actor)) {
        target_actors.Add(actor);

        // If we're already playing, update the payload immediately
        if (HasActorBegunPlay()) {
            register_targets_with_payload();
        }
    }
}

void ATriggerButtonActor::clear_targets() {
    target_actors.Empty();
    trigger_payload.targets = {};
    trigger_payload.n_targets = 0;
}

void ATriggerButtonActor::register_targets_with_payload() {
    // Clear existing targets
    trigger_payload.targets = {};
    trigger_payload.n_targets = 0;

    auto* subsystem{GetWorld()->GetSubsystem<UTriggerSubsystem>()};
    if (!subsystem) {
        log_warning(TEXT("No UTriggerSubsystem found for target registration"));
        return;
    }

    for (auto* target_actor : target_actors) {
        if (!target_actor) {
            continue;
        }

        // Get the TriggerableId for this actor
        if (auto target_id{subsystem->get_triggerable_id(target_actor)}) {
            trigger_payload.add_target(*target_id);
            log_verbose(TEXT("Added target: %s"), *target_actor->GetName());
        } else {
            log_warning(TEXT("Actor %s is not registered with trigger subsystem"),
                        *target_actor->GetName());
        }
    }

    log_log(TEXT("Registered %d targets with trigger button"), trigger_payload.n_targets);
}
