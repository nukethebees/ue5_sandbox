#include <ioj/sim/entity_registry.h>
#include <ioj/sim/overlap_handler.h>
#include "support/collision_agent_storage.h"
#include "support/simulation_test_support.h"

namespace ioj::sim::tests {

namespace {
auto spawn_entity(EntityRegistry& registry, EntityType const type) -> RegistryEntityHandle {
    EntityRegistry::EntityData data;
    data.add_defaulted(1);
    data.healths[0] = 100;
    data.teams[0] = Team::Blue;
    data.entity_types[0] = type;
    return registry.add_entities(data.get_const_view()).get_handle(0);
}

void mark_dead(EntityRegistry& registry, RegistryEntityHandle const handle) {
    auto const& current{registry.get_entity_data()};
    EntityRegistry::EntityData update;
    update.add_defaulted(1);
    update.copy_element(0, current, handle.index);
    update.healths[0] = 0;
    EntityDeathInfo death;
    death.add(DeathReason::Unknown, handle, {});
    registry.queue_entity_updates({std::array{handle}, update.get_const_view()}, death);
    registry.commit_updates();
}
}

TEST(OverlapHandler, QueuesEnvironmentalDamageForEachSupportedOverlapParticipant) {

    EntityRegistry registry;
    auto const capital{spawn_entity(registry, EntityType::CapitalShip)};
    auto const fighter{spawn_entity(registry, EntityType::Fighter)};
    auto const turret{spawn_entity(registry, EntityType::Turret)};

    collision::EntityEntityOverlaps entity_overlaps;
    entity_overlaps.add(registry.get_current_id(capital), registry.get_current_id(fighter));
    entity_overlaps.add(registry.get_current_id(fighter), registry.get_current_id(turret));
    collision::EntityStaticOverlaps static_overlaps;
    static_overlaps.add(registry.get_current_id(fighter), 7);

    CollisionAgentStorage owners;
    owners.load(registry);
    OverlapHandler handler{registry, owners.agents, {.damage_per_overlap_detection = 37}};
    handler.handle({entity_overlaps.get_const_view(), static_overlaps.get_const_view()});

    auto const& damage{registry.get_direct_damage_queue_view()};
    tests::expect_equal(damage.num(), 5, "Every supported overlap endpoint queues damage");
    std::int32_t capital_count{};
    std::int32_t fighter_count{};
    std::int32_t turret_count{};
    for (std::int32_t index{}; index < damage.num(); ++index) {
        tests::expect_equal(damage.damage_amounts[index], 37, "Configured overlap damage is used");
        tests::expect_true(!damage.instigators[index].is_valid(),
                           "Overlap damage has no combat instigator");
        capital_count += damage.damaged_entities[index] == registry.get_current_id(capital) ? 1 : 0;
        fighter_count += damage.damaged_entities[index] == registry.get_current_id(fighter) ? 1 : 0;
        turret_count += damage.damaged_entities[index] == registry.get_current_id(turret) ? 1 : 0;
    }
    tests::expect_equal(capital_count, 1, "Capital receives its pair contribution");
    tests::expect_equal(fighter_count, 3, "Fighter receives all three contributions");
    tests::expect_equal(turret_count, 1, "Turret receives its pair contribution");
    tests::expect_equal(
        registry.get_combat_telemetry()
            .hits[std::to_underlying(Team::Blue)][std::to_underlying(EntityType::CapitalShip)],
        std::uint64_t{0},
        "Environmental damage gives no combat hits");
}

TEST(OverlapHandler, SkipsUnsupportedDeadInvalidAndStaleRecipients) {

    EntityRegistry registry;
    auto const fighter{spawn_entity(registry, EntityType::Fighter)};
    auto const spinner{spawn_entity(registry, EntityType::TubeSpinner)};
    auto const doomed{spawn_entity(registry, EntityType::Turret)};
    auto const doomed_id{registry.get_current_id(doomed)};
    mark_dead(registry, doomed);

    collision::EntityEntityOverlaps first_pairs;
    first_pairs.add(registry.get_current_id(fighter), registry.get_current_id(spinner));
    first_pairs.add(registry.get_current_id(fighter), doomed_id);
    first_pairs.add(registry.get_current_id(fighter), EntityUniqueId{});
    CollisionAgentStorage owners;
    owners.load(registry);
    OverlapHandler handler{registry, owners.agents, {}};
    handler.handle({first_pairs.get_const_view(), {}});

    auto const& first_damage{registry.get_direct_damage_queue_view()};
    tests::expect_equal(
        first_damage.num(), 3, "Only the live supported endpoint is damaged per pair");
    for (auto const recipient : first_damage.damaged_entities) {
        tests::expect_true(recipient == registry.get_current_id(fighter),
                           "Skipped endpoints never enter the damage queue");
    }

    registry.end_tick();
    auto const replacement{spawn_entity(registry, EntityType::Turret)};
    tests::expect_true(registry.is_stale(doomed), "Dead slot reuse makes the old handle stale");

    owners.load(registry);
    collision::EntityEntityOverlaps stale_pair;
    stale_pair.add(registry.get_current_id(fighter), doomed_id);
    handler.handle({stale_pair.get_const_view(), {}});
    auto const& second_damage{registry.get_direct_damage_queue_view()};
    tests::expect_equal(second_damage.num(), 1, "Stale endpoint is skipped independently");
    tests::expect_true(second_damage.damaged_entities[0] != registry.get_current_id(replacement),
                       "Replacement is not accidentally damaged");
    registry.end_tick();
    owners.fighters.get_view().healths()[0] = 0;
    EXPECT_TRUE(registry.is_valid_alive(fighter));
    handler.handle({stale_pair.get_const_view(), {}});
    EXPECT_EQ(registry.get_direct_damage_queue_view().num(), 0);
}

} // namespace tests
