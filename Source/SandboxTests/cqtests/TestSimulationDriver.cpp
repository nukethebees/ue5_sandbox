#include "TestSimulationDriver.h"

#include <Sandbox/batch_game/test_entity_registry/DamageEvents.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/constants/collision_channels.h>

#include <Components/PrimitiveComponent.h>
#include <Engine/HitResult.h>
#include <Engine/World.h>

namespace ml {
void TestSimulationDriver::queue_damage(FRegistryEntityHandle const target,
                                        int32 const damage,
                                        FRegistryEntityHandle const instigator) {
    // To avoid creating new code paths, we need to do a "collision" to do damage

    check(registry.is_valid_alive(target));
    FVector location{registry.get_location(target)};
    FVector const half_trace{0.0, 0.0, 1.0};

    FHitResult hit;
    auto const did_hit{world.LineTraceSingleByChannel(
        hit,
        location - half_trace,
        location + half_trace,
        ml::collision::projectile,
        FCollisionQueryParams{SCENE_QUERY_STAT(TestDamageInjector), false})};

    check(did_hit);

    UnresolvedDamageEvents damage_events;
    damage_events.add_uninitialised(1);

    damage_events.damaged_actors[0] = hit.GetActor();
    damage_events.damage_amounts[0] = damage;
    damage_events.actor_components[0] = hit.GetComponent();
    damage_events.hit_items[0] = hit.Item;
    damage_events.instigators[0] = instigator;

    registry.queue_damage_events(damage_events);
}
}
