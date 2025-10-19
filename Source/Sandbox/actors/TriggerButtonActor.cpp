#include "Sandbox/actors/TriggerButtonActor.h"

#include "Components/SceneComponent.h"

#include "Sandbox/subsystems/world/TriggerSubsystem.h"
#include "Sandbox/utilities/actor_utils.h"

#include "Sandbox/macros/null_checks.hpp"

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
    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(subsystem, world->GetSubsystem<UTriggerSubsystem>());

    if (auto const id{subsystem->register_triggerable(*this, std::move(trigger_payload))}) {
        my_trigger_id = *id;
    }
}

void ATriggerButtonActor::EndPlay(EEndPlayReason::Type reason) {
    if (auto* subsystem{GetWorld()->GetSubsystem<UTriggerSubsystem>()}) {
        subsystem->deregister_triggerable(*this);
    }
    Super::EndPlay(reason);
}

void ATriggerButtonActor::register_targets_with_payload() {
    constexpr auto logger{NestedLogger<"register_targets_with_payload">()};

    // Clear existing targets
    trigger_payload.target_actor_ids = {};
    trigger_payload.n_targets = 0;

    TRY_INIT_PTR(world, GetWorld());
    TRY_INIT_PTR(subsystem, world->GetSubsystem<UTriggerSubsystem>());

    for (auto* target_actor : target_actors) {
        CONTINUE_IF_FALSE(target_actor);

        // Get or create actor ID for this target
        auto const actor_id{subsystem->get_or_create_actor_id(*target_actor)};

        CONTINUE_IF_FALSE(trigger_payload.add_target_actor(actor_id));

        logger.log_verbose(TEXT("Added target actor ID %llu for actor: %s"),
                           actor_id.get(),
                           *ml::get_best_display_name(*target_actor));
    }

    logger.log_log(TEXT("Registered %d target actors with trigger button: %s"),
                   trigger_payload.n_targets,
                   *ml::get_best_display_name(*this));
}
