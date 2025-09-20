#include "Sandbox/data/CollisionPayloads.h"

void FSpeedBoostPayload::execute(AActor& actor) {
    if (auto* boost_component{actor.FindComponentByClass<USpeedBoostComponent>()}) {
        boost_component->apply_speed_boost(speed_boost);
    }
}

void FJumpIncreasePayload::execute(AActor& actor) {
    UE_LOGFMT(LogTemp, Verbose, "FJumpIncreasePayload::execute");
    if (auto* character{Cast<AMyCharacter>(&actor)}) {
        character->increase_max_jump_count(jump_count_increase);
    } else {
        UE_LOGFMT(LogTemp, Warning, "FJumpIncreasePayload: Could not cast the actor to character.");
    }
}

void FCoinPayload::execute(AActor& actor) {
    if (auto* const player{Cast<AMyCharacter>(&actor)}) {
        if (auto* const coin_collector{
                player->FindComponentByClass<UCoinCollectorActorComponent>()}) {
            coin_collector->add_coins(value);
        }

        if (auto* world{actor.GetWorld()}) {
            auto* destruction_manager{world->GetSubsystem<UDestructionManagerSubsystem>()};
            if (!destruction_manager) {
                return;
            }
        }
    }
}
