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
        if (auto const id{subsystem->register_triggerable(*this, std::move(trigger_payload))}) {
            my_trigger_id = *id;
        }
    }
}

void ATriggerButtonActor::EndPlay(EEndPlayReason::Type reason) {
    if (auto* subsystem{GetWorld()->GetSubsystem<UTriggerSubsystem>()}) {
        subsystem->deregister_triggerable(*this);
    }
    Super::EndPlay(reason);
}

void ATriggerButtonActor::register_targets_with_payload() {
    // Clear existing targets
    trigger_payload.target_actor_ids = {};
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

        // Get or create actor ID for this target
        auto const actor_id{subsystem->get_or_create_actor_id(*target_actor)};
        trigger_payload.add_target_actor(actor_id);
        log_verbose(TEXT("Added target actor ID %llu for actor: %s"),
                    actor_id.get(),
                    *target_actor->GetActorLabel());
    }

    log_log(TEXT("Registered %d target actors with trigger button: %s"),
            trigger_payload.n_targets,
            *GetActorLabel());
}
