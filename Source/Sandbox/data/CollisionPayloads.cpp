#include "Sandbox/data/CollisionPayloads.h"

void FSpeedBoostPayload::execute(FCollisionContext context) {
    if (auto* boost_component{
            context.collided_actor.FindComponentByClass<USpeedBoostComponent>()}) {
        boost_component->apply_speed_boost(speed_boost);
    }
}

void FJumpIncreasePayload::execute(FCollisionContext context) {
    UE_LOGFMT(LogTemp, Verbose, "FJumpIncreasePayload::execute");
    if (auto* character{Cast<AMyCharacter>(&context.collided_actor)}) {
        character->increase_max_jump_count(jump_count_increase);
    } else {
        UE_LOGFMT(LogTemp, Warning, "FJumpIncreasePayload: Could not cast the actor to character.");
    }
}

void FCoinPayload::execute(FCollisionContext context) {
    if (auto* const player{Cast<AMyCharacter>(&context.collided_actor)}) {
        if (auto* const coin_collector{
                player->FindComponentByClass<UCoinCollectorActorComponent>()}) {
            coin_collector->add_coins(value);
        }

        auto* destruction_manager{context.world.GetSubsystem<UDestructionManagerSubsystem>()};
        if (!destruction_manager) {
            return;
        }
    }
}
