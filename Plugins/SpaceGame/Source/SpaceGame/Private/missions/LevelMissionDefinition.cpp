#include <SpaceGame/missions/LevelMissionDefinition.h>

void FTestMissionStartupData::prune_invalid_actors() {
    hero_entities.RemoveAll([](TObjectPtr<AActor> const& actor) { return !IsValid(actor); });
    entities_must_survive.RemoveAll(
        [](TObjectPtr<AActor> const& actor) { return !IsValid(actor); });
    entities_required_to_kill.RemoveAll(
        [](TObjectPtr<AActor> const& actor) { return !IsValid(actor); });
}
void FLevelMissionDefinition::replace_startup_actor(AActor const* const old_actor,
                                                    AActor& new_actor) {
    if (!old_actor) {
        return;
    }

    auto replace_actor{[old_actor, &new_actor](TObjectPtr<AActor>& actor) {
        if (actor == old_actor) {
            actor = &new_actor;
        }
    }};

    for (auto& actor : startup_data.hero_entities) {
        replace_actor(actor);
    }
    for (auto& actor : startup_data.entities_must_survive) {
        replace_actor(actor);
    }
    for (auto& actor : startup_data.entities_required_to_kill) {
        replace_actor(actor);
    }
}
