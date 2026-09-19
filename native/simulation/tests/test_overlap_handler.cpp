#include <ioj/sim/overlap_handler.h>
#include "support/collision_agent_storage.h"
#include "support/simulation_test_support.h"

namespace ioj::sim::tests {

TEST(OverlapHandler, QueuesEnvironmentalDamageForEachSupportedOverlapParticipant) {

    CollisionAgentStorage owners;
    auto const capital{owners.spawn(EntityType::CapitalShip)};
    auto const fighter{owners.spawn(EntityType::Fighter)};
    auto const turret{owners.spawn(EntityType::Turret)};
    owners.publish();

    collision::EntityEntityOverlaps entity_overlaps;
    entity_overlaps.add(capital, fighter);
    entity_overlaps.add(fighter, turret);
    collision::EntityStaticOverlaps static_overlaps;
    static_overlaps.add(fighter, 7);

    OverlapHandler handler{
        owners.combat_events, owners.agents, {.damage_per_overlap_detection = 37}};
    handler.handle({entity_overlaps.get_const_view(), static_overlaps.get_const_view()});

    auto const damage{owners.combat_events.all_events().get_const_view()};
    tests::expect_equal(damage.num(), 5, "Every supported overlap endpoint queues damage");
    std::int32_t capital_count{};
    std::int32_t fighter_count{};
    std::int32_t turret_count{};
    for (std::int32_t index{}; index < damage.num(); ++index) {
        tests::expect_equal(damage.damage_amounts[index], 37, "Configured overlap damage is used");
        tests::expect_true(!damage.instigators[index].is_valid(),
                           "Overlap damage has no combat instigator");
        capital_count += damage.damaged_entities[index] == capital ? 1 : 0;
        fighter_count += damage.damaged_entities[index] == fighter ? 1 : 0;
        turret_count += damage.damaged_entities[index] == turret ? 1 : 0;
    }
    tests::expect_equal(capital_count, 1, "Capital receives its pair contribution");
    tests::expect_equal(fighter_count, 3, "Fighter receives all three contributions");
    tests::expect_equal(turret_count, 1, "Turret receives its pair contribution");
    tests::expect_equal(
        owners.ledger.get_combat_telemetry()
            .hits[std::to_underlying(Team::Blue)][std::to_underlying(EntityType::CapitalShip)],
        std::uint64_t{0},
        "Environmental damage gives no combat hits");
}

TEST(OverlapHandler, SkipsUnsupportedDeadInvalidAndRetiredRecipients) {

    CollisionAgentStorage owners;
    auto const fighter{owners.spawn(EntityType::Fighter)};
    auto const spinner{owners.spawn(EntityType::TubeSpinner)};
    auto const doomed{owners.spawn(EntityType::Turret, {}, {}, 0)};
    owners.publish();

    collision::EntityEntityOverlaps first_pairs;
    first_pairs.add(fighter, spinner);
    first_pairs.add(fighter, doomed);
    first_pairs.add(fighter, EntityUniqueId{});
    OverlapHandler handler{owners.combat_events, owners.agents, {}};
    handler.handle({first_pairs.get_const_view(), {}});

    auto const first_damage{owners.combat_events.all_events().get_const_view()};
    tests::expect_equal(
        first_damage.num(), 3, "Only the live supported endpoint is damaged per pair");
    for (auto const recipient : first_damage.damaged_entities) {
        tests::expect_true(recipient == fighter, "Skipped endpoints never enter the damage queue");
    }

    owners.combat_events.reset();
    owners.remove(doomed);
    auto const replacement{owners.spawn(EntityType::Turret)};
    owners.publish();
    collision::EntityEntityOverlaps retired_pair;
    retired_pair.add(fighter, doomed);
    handler.handle({retired_pair.get_const_view(), {}});
    auto const second_damage{owners.combat_events.all_events().get_const_view()};
    tests::expect_equal(second_damage.num(), 1, "Retired endpoint is skipped independently");
    tests::expect_true(second_damage.damaged_entities[0] != replacement,
                       "Replacement is not accidentally damaged");

    owners.combat_events.reset();
    auto const fighter_data{owners.fighters.get_const_view().columns()};
    owners.health_table.get_view(fighter_data.health_indices, fighter_data.entity_ids).health(0) =
        0;
    handler.handle({retired_pair.get_const_view(), {}});
    EXPECT_EQ(owners.combat_events.all_events().num(), 0);
}

} // namespace ioj::sim::tests
