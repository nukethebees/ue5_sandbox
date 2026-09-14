#include <ioj/sim/entity_registry.h>
#include <ioj/sim/overlap_handler.h>
#include "support/simulation_test_support.h"

namespace ioj::sim::tests {

namespace {
auto spawn_entity(EntityRegistry& registry, ioj::sim::EntityType const type)
    -> RegistryEntityHandle {
    EntityRegistry::EntityData data;
    data.add_defaulted(1);
    data.healths[0] = 100;
    data.teams[0] = ioj::sim::Team::Blue;
    data.entity_types[0] = type;
    data.alive[0] = 1;
    return registry.add_entities(data.get_const_view()).get_handle(0);
}

void mark_dead(EntityRegistry& registry, RegistryEntityHandle const handle) {
    auto const& current{registry.get_entity_data()};
    EntityRegistry::EntityData update;
    update.add_defaulted(1);
    update.copy_element(0, current, handle.index);
    update.healths[0] = 0;
    update.alive[0] = 0;
    EntityDeathInfo death;
    death.add(ioj::sim::DeathReason::Unknown, handle, {});
    registry.queue_entity_updates({std::array{handle}, update.get_const_view()}, death);
    registry.commit_updates();
}
}

TEST(OverlapHandler, QueuesEnvironmentalDamageForEachSupportedOverlapParticipant) {

    EntityRegistry registry;
    auto const capital{spawn_entity(registry, ioj::sim::EntityType::CapitalShip)};
    auto const fighter{spawn_entity(registry, ioj::sim::EntityType::Fighter)};
    auto const turret{spawn_entity(registry, ioj::sim::EntityType::Turret)};

    ioj::sim::collision::EntityEntityOverlaps entity_overlaps;
    entity_overlaps.add(capital, fighter);
    entity_overlaps.add(fighter, turret);
    ioj::sim::collision::EntityStaticOverlaps static_overlaps;
    static_overlaps.add(fighter, 7);

    ioj::sim::OverlapHandler handler{registry, {.damage_per_overlap_detection = 37}};
    handler.handle({entity_overlaps.get_const_view(), static_overlaps.get_const_view()});

    auto const& damage{registry.get_direct_damage_queue_view()};
    ioj::sim::tests::expect_equal(
        damage.num(), 5, "Every supported overlap endpoint queues damage");
    std::int32_t capital_count{};
    std::int32_t fighter_count{};
    std::int32_t turret_count{};
    for (std::int32_t index{}; index < damage.num(); ++index) {
        ioj::sim::tests::expect_equal(
            damage.damage_amounts[index], 37, "Configured overlap damage is used");
        ioj::sim::tests::expect_true(damage.instigators[index].is_null(),
                                     "Overlap damage has no combat instigator");
        capital_count += damage.damaged_entities[index] == capital ? 1 : 0;
        fighter_count += damage.damaged_entities[index] == fighter ? 1 : 0;
        turret_count += damage.damaged_entities[index] == turret ? 1 : 0;
    }
    ioj::sim::tests::expect_equal(capital_count, 1, "Capital receives its pair contribution");
    ioj::sim::tests::expect_equal(fighter_count, 3, "Fighter receives all three contributions");
    ioj::sim::tests::expect_equal(turret_count, 1, "Turret receives its pair contribution");
    ioj::sim::tests::expect_equal(
        registry.get_combat_telemetry().hits[std::to_underlying(ioj::sim::Team::Blue)]
                                            [std::to_underlying(ioj::sim::EntityType::CapitalShip)],
        std::uint64_t{0},
        "Environmental damage gives no combat hits");
}

TEST(OverlapHandler, SkipsUnsupportedDeadInvalidAndStaleRecipients) {

    EntityRegistry registry;
    auto const fighter{spawn_entity(registry, ioj::sim::EntityType::Fighter)};
    auto const spinner{spawn_entity(registry, ioj::sim::EntityType::TubeSpinner)};
    auto const doomed{spawn_entity(registry, ioj::sim::EntityType::Turret)};
    mark_dead(registry, doomed);

    ioj::sim::collision::EntityEntityOverlaps first_pairs;
    first_pairs.add(fighter, spinner);
    first_pairs.add(fighter, doomed);
    first_pairs.add(fighter, RegistryEntityHandle{999, 0});
    ioj::sim::OverlapHandler handler{registry, {}};
    handler.handle({first_pairs.get_const_view(), {}});

    auto const& first_damage{registry.get_direct_damage_queue_view()};
    ioj::sim::tests::expect_equal(
        first_damage.num(), 3, "Only the live supported endpoint is damaged per pair");
    for (auto const recipient : first_damage.damaged_entities) {
        ioj::sim::tests::expect_true(recipient == fighter,
                                     "Skipped endpoints never enter the damage queue");
    }

    registry.end_tick();
    auto const replacement{spawn_entity(registry, ioj::sim::EntityType::Turret)};
    ioj::sim::tests::expect_true(registry.is_stale(doomed),
                                 "Dead slot reuse makes the old handle stale");

    ioj::sim::collision::EntityEntityOverlaps stale_pair;
    stale_pair.add(fighter, doomed);
    handler.handle({stale_pair.get_const_view(), {}});
    auto const& second_damage{registry.get_direct_damage_queue_view()};
    ioj::sim::tests::expect_equal(
        second_damage.num(), 1, "Stale endpoint is skipped independently");
    ioj::sim::tests::expect_true(second_damage.damaged_entities[0] != replacement,
                                 "Replacement is not accidentally damaged");
}

} // namespace ioj::sim::tests
