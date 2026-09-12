#include <SpaceGameSimulation/combat/OverlapHandler.h>
#include <SpaceGameSimulation/entities/TestEntityRegistry.h>

#include <CQTest.h>

namespace {
auto spawn_entity(FTestEntityRegistry& registry, ETestEntityType const type)
    -> FRegistryEntityHandle {
    FTestEntityRegistry::EntityData data;
    data.locations.add(FVector3f::ZeroVector);
    data.velocities.add(FVector3f::ZeroVector);
    data.rotations.add(0.f, 0.f, 0.f);
    data.radii.Add(1.f);
    data.healths.Add(100);
    data.teams.Add(ETestTeam::Blue);
    data.entity_types.Add(type);
    data.alive.Add(uint8{1});
    return registry.add_entities(data.get_const_view()).registry_handles[0];
}

void mark_dead(FTestEntityRegistry& registry, FRegistryEntityHandle const handle) {
    auto const& current{registry.get_entity_data()};
    FTestEntityRegistry::EntityData update;
    update.locations.add(current.locations[handle.index]);
    update.velocities.add(current.velocities[handle.index]);
    update.rotations.add(current.rotations.pitches[handle.index],
                         current.rotations.yaws[handle.index],
                         current.rotations.rolls[handle.index]);
    update.radii.Add(current.radii[handle.index]);
    update.healths.Add(0);
    update.teams.Add(current.teams[handle.index]);
    update.entity_types.Add(current.entity_types[handle.index]);
    update.alive.Add(uint8{0});
    EntityDeathInfo death;
    death.add(ETestDeathReason::Unknown, handle);
    registry.queue_entity_updates({TArray{handle}, update.get_const_view()}, death);
    registry.commit_updates();
}
}

TEST_CLASS(OverlapHandler, "Sandbox.UnitTests")
{
    TEST_METHOD(QueuesEnvironmentalDamageForEachSupportedOverlapParticipant)
    {
        FTestEntityRegistry registry;
        auto const capital{spawn_entity(registry, ETestEntityType::CapitalShip)};
        auto const fighter{spawn_entity(registry, ETestEntityType::CapitalShipFighter)};
        auto const turret{spawn_entity(registry, ETestEntityType::Turret)};

        ml::ioj::FEntityEntityOverlaps entity_overlaps;
        entity_overlaps.add(capital, fighter);
        entity_overlaps.add(fighter, turret);
        ml::ioj::FEntityStaticOverlaps static_overlaps;
        static_overlaps.add(fighter, 7);

        ml::FOverlapHandler handler{registry, {.damage_per_overlap_detection = 37}};
        handler.handle({entity_overlaps.get_const_view(), static_overlaps.get_const_view()});

        auto const& damage{registry.get_direct_damage_queue_view()};
        TestRunner->TestEqual(
            TEXT("Every supported overlap endpoint queues damage"), damage.num(), 5);
        int32 capital_count{};
        int32 fighter_count{};
        int32 turret_count{};
        for (int32 index{}; index < damage.num(); ++index) {
            TestRunner->TestEqual(
                TEXT("Configured overlap damage is used"), damage.damage_amounts[index], 37);
            TestRunner->TestTrue(TEXT("Overlap damage has no combat instigator"),
                                 damage.instigators[index].is_null());
            capital_count += damage.damaged_entities[index] == capital ? 1 : 0;
            fighter_count += damage.damaged_entities[index] == fighter ? 1 : 0;
            turret_count += damage.damaged_entities[index] == turret ? 1 : 0;
        }
        TestRunner->TestEqual(TEXT("Capital receives its pair contribution"), capital_count, 1);
        TestRunner->TestEqual(TEXT("Fighter receives all three contributions"), fighter_count, 3);
        TestRunner->TestEqual(TEXT("Turret receives its pair contribution"), turret_count, 1);
        TestRunner->TestEqual(
            TEXT("Environmental damage gives no combat hits"),
            registry.get_combat_telemetry().hits[std::to_underlying(ETestTeam::Blue)]
                                                [std::to_underlying(ETestEntityType::CapitalShip)],
            uint64{0});
    }

    TEST_METHOD(SkipsUnsupportedDeadInvalidAndStaleRecipients)
    {
        FTestEntityRegistry registry;
        auto const fighter{spawn_entity(registry, ETestEntityType::CapitalShipFighter)};
        auto const spinner{spawn_entity(registry, ETestEntityType::TubeSpinner)};
        auto const doomed{spawn_entity(registry, ETestEntityType::Turret)};
        mark_dead(registry, doomed);

        ml::ioj::FEntityEntityOverlaps first_pairs;
        first_pairs.add(fighter, spinner);
        first_pairs.add(fighter, doomed);
        first_pairs.add(fighter, FRegistryEntityHandle{999, 0});
        ml::FOverlapHandler handler{registry, {}};
        handler.handle({first_pairs.get_const_view(), {}});

        auto const& first_damage{registry.get_direct_damage_queue_view()};
        TestRunner->TestEqual(
            TEXT("Only the live supported endpoint is damaged per pair"), first_damage.num(), 3);
        for (auto const recipient : first_damage.damaged_entities) {
            TestRunner->TestTrue(TEXT("Skipped endpoints never enter the damage queue"),
                                 recipient == fighter);
        }

        registry.end_tick();
        auto const replacement{spawn_entity(registry, ETestEntityType::Turret)};
        TestRunner->TestTrue(TEXT("Dead slot reuse makes the old handle stale"),
                             registry.is_stale(doomed));

        ml::ioj::FEntityEntityOverlaps stale_pair;
        stale_pair.add(fighter, doomed);
        handler.handle({stale_pair.get_const_view(), {}});
        auto const& second_damage{registry.get_direct_damage_queue_view()};
        TestRunner->TestEqual(
            TEXT("Stale endpoint is skipped independently"), second_damage.num(), 1);
        TestRunner->TestTrue(TEXT("Replacement is not accidentally damaged"),
                             second_damage.damaged_entities[0] != replacement);
    }
};
