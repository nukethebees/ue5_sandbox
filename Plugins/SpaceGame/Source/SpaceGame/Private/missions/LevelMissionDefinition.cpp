#include <SpaceGame/entities/TestEntity.h>
#include <SpaceGame/entities/TestEntityRegistry.h>
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

void FLevelMissionDefinition::apply(FTestMissionManager& mission,
                                    FProxyEntityMap const& proxies,
                                    FTestEntityRegistry const& registry) const {
    mission.set_mission_mode(mission_mode);
    mission.set_target_time(target_time);
    mission.set_kill_target(kill_target);
    mission.set_save_mission_results(save_mission_results);
    mission.set_level_identity(level_id, level_display_name);
    auto resolve{[&](AActor const& actor) {
        if (auto const* identifiers{proxies.Find(&actor)}) {
            return identifiers->handle;
        }
        auto const* entity{Cast<ITestEntity>(&actor)};
        check(entity);
        auto const handle{entity->get_entity_handle()};
        check(registry.is_valid_handle(handle));
        return handle;
    }};
    for (auto const actor : startup_data.hero_entities) {
        if (IsValid(actor)) {
            mission.add_hero_entity(resolve(*actor));
        }
    }
    for (auto const actor : startup_data.entities_must_survive) {
        if (IsValid(actor)) {
            mission.add_entity_that_must_survive(resolve(*actor));
        }
    }
    for (auto const actor : startup_data.entities_required_to_kill) {
        if (IsValid(actor)) {
            mission.add_entity_required_to_kill(resolve(*actor));
        }
    }
}
