#include <ioj/sim/agent_indexes.h>
#include <ioj/sim/batch_operations.h>
#include <ioj/sim/combat_events.h>
#include <ioj/sim/damage_queue.h>
#include <ioj/sim/entity_death_info.h>
#include <ioj/sim/entity_ledger.h>
#include <sandbox/core/frame_memory_resource.h>

#include <array>
#include <gtest/gtest.h>
#include <utility>
#include <vector>

namespace ioj::sim::tests {
namespace {
auto make_id(EntityType const type, std::uint32_t const ordinal) -> EntityUniqueId {
    return EntityUniqueId::make(entity_identity_offset(type, ordinal), type);
}
}

TEST(DamageQueue, GroupsMixedOwnerEventsAndFiltersRetiredRecipients) {
    SimClock clock;
    AgentIndexes indexes{clock};
    auto const turret{make_id(EntityType::Turret, 0)};
    auto const capital{make_id(EntityType::CapitalShip, 0)};
    auto const retired_fighter{make_id(EntityType::Fighter, 0)};
    std::array const turrets{turret};
    std::array const capitals{capital};
    indexes.bind(EntityType::Turret, turrets);
    indexes.bind(EntityType::CapitalShip, capitals);

    DirectDamageEvents first;
    first.add(capital, 5, turret);
    first.add(turret, 8, {});
    DirectDamageEvents second;
    second.add(capital, 13, turret);
    second.add(retired_fighter, 21, capital);

    DamageQueue queue;
    queue.append(first.get_const_view());
    queue.append(second.get_const_view());
    first.damage_amounts[0] = 999;

    alignas(ml::FrameMemoryResource::backing_alignment) std::array<std::byte, 4096> backing;
    ml::FrameMemoryResource frame_memory{backing};
    {
        ml::FrameScratchScope scratch_scope{frame_memory};
        queue.prepare(indexes, scratch_scope.scratch());
    }

    auto const turret_events{queue.events_for(EntityType::Turret)};
    ASSERT_EQ(turret_events.num(), 1);
    EXPECT_EQ(turret_events.damaged_entities[0], turret);
    EXPECT_EQ(turret_events.damage_amounts[0], 8);
    EXPECT_FALSE(turret_events.instigators[0].is_valid());

    auto const capital_events{queue.events_for(EntityType::CapitalShip)};
    ASSERT_EQ(capital_events.num(), 2);
    EXPECT_EQ(capital_events.damaged_entities[0], capital);
    EXPECT_EQ(capital_events.damage_amounts[0], 5);
    EXPECT_EQ(capital_events.damage_amounts[1], 13);
    EXPECT_EQ(capital_events.instigators[0], turret);
    EXPECT_EQ(capital_events.instigators[1], turret);

    EXPECT_TRUE(queue.events_for(EntityType::Fighter).is_empty());
    EXPECT_EQ(queue.all_events().num(), 3);

    queue.reset();
    EXPECT_TRUE(queue.all_events().is_empty());
    EXPECT_TRUE(queue.events_for(EntityType::CapitalShip).is_empty());
}

TEST(DamageResolution, RecordsOnlyDamageAppliedToLiveEntities) {
    EntityLedger ledger;
    auto const attacker{ledger.record_spawn(EntityType::Turret, Team::Red, true)};
    auto const victim{ledger.record_spawn(EntityType::Fighter, Team::Blue, true)};
    auto const retired{ledger.record_spawn(EntityType::CapitalShip, Team::Blue, true)};

    SimClock clock;
    AgentIndexes indexes{clock};
    std::array const fighters{victim};
    indexes.bind(EntityType::Fighter, fighters);

    DirectDamageEvents queued;
    queued.add(retired, 100, attacker);
    queued.add(victim, 6, attacker);
    queued.add(victim, 10, attacker);
    queued.add(victim, 100, attacker);

    CombatEvents events{ledger};
    events.queue_damage(queued);

    auto const red{std::to_underlying(Team::Red)};
    auto const blue{std::to_underlying(Team::Blue)};
    auto const turret_type{std::to_underlying(EntityType::Turret)};
    auto const fighter_type{std::to_underlying(EntityType::Fighter)};
    EXPECT_EQ(ledger.get_combat_telemetry().hits[red][turret_type], 0u);

    alignas(ml::FrameMemoryResource::backing_alignment) std::array<std::byte, 4096> backing;
    ml::FrameMemoryResource frame_memory{backing};
    {
        ml::FrameScratchScope scratch_scope{frame_memory};
        events.prepare(indexes, scratch_scope.scratch());
    }
    ASSERT_EQ(events.all_events().num(), 3);

    std::array ids{victim};
    std::array<Health, 1> initial_healths{10};
    std::array<HealthIndex, 1> health_indices{};
    HealthTable health_table;
    health_table.add(ids, initial_healths, health_indices);
    std::vector<std::int32_t> removals;
    EntityDeathInfo deaths;
    batch::resolve_damage_events(events.events_for(EntityType::Fighter),
                                 indexes,
                                 ids,
                                 health_table.get_view(health_indices, ids),
                                 removals,
                                 deaths,
                                 ledger);

    EXPECT_EQ(health_table.get_health(health_indices[0], ids[0]), -6);
    EXPECT_EQ(removals, std::vector<std::int32_t>{0});
    ASSERT_EQ(deaths.num(), 1);
    EXPECT_EQ(deaths.victims[0], victim);
    EXPECT_EQ(deaths.killers[0], attacker);

    auto const& telemetry{ledger.get_combat_telemetry()};
    EXPECT_EQ(telemetry.hits[red][turret_type], 2u);
    EXPECT_DOUBLE_EQ(telemetry.damage_dealt[red][turret_type], 10.0);
    EXPECT_DOUBLE_EQ(telemetry.damage_received[blue][fighter_type], 10.0);
}

TEST(EntityLedger, PreservesHistoricalAccountingAndResetsLevelIdentity) {
    EntityLedger ledger;
    auto const killer{ledger.record_spawn(EntityType::Turret, Team::Red, true)};
    auto const first_victim{ledger.record_spawn(EntityType::CapitalShip, Team::Blue, true)};

    ledger.record_shots(std::array{killer});
    ledger.record_status(killer, Team::Green, true);
    ledger.record_status(first_victim, Team::Yellow, true);
    ledger.record_shots(std::array{killer, EntityUniqueId{}});

    ledger.record_damage(first_victim, killer, 17);
    ledger.record_death(first_victim, killer, DeathReason::Combat);
    ledger.record_death(killer, {}, DeathReason::Unknown);

    auto const second_victim{ledger.record_spawn(EntityType::Fighter, Team::Blue, true)};
    ledger.record_death(second_victim, killer, DeathReason::Combat);

    EXPECT_EQ(ledger.get_kills(killer), 2u);
    EXPECT_EQ(ledger.count_kills(), 2);
    EXPECT_EQ(ledger.count_alive(), 0);
    EXPECT_EQ(ledger.get_unique_entities().killed_by[ledger.get_history_index(first_victim)],
              killer);
    EXPECT_EQ(ledger.get_unique_entities().killed_by[ledger.get_history_index(second_victim)],
              killer);

    auto const& telemetry{ledger.get_combat_telemetry()};
    auto const red{std::to_underlying(Team::Red)};
    auto const green{std::to_underlying(Team::Green)};
    auto const yellow{std::to_underlying(Team::Yellow)};
    auto const turret_type{std::to_underlying(EntityType::Turret)};
    auto const capital_type{std::to_underlying(EntityType::CapitalShip)};
    EXPECT_EQ(telemetry.shots[red][turret_type], 1u);
    EXPECT_EQ(telemetry.shots[green][turret_type], 1u);
    EXPECT_EQ(telemetry.hits[green][turret_type], 1u);
    EXPECT_DOUBLE_EQ(telemetry.damage_dealt[green][turret_type], 17.0);
    EXPECT_DOUBLE_EQ(telemetry.damage_received[yellow][capital_type], 17.0);
    EXPECT_EQ(telemetry.kills[green][turret_type], 2u);
    EXPECT_EQ(telemetry.destroyed[yellow][capital_type], 1u);

    ledger.reset();
    EXPECT_EQ(ledger.get_num_unique_ids_issued(), 0);
    EXPECT_EQ(ledger.count_alive(), 0);
    EXPECT_EQ(ledger.count_kills(), 0);
    EXPECT_FALSE(ledger.is_valid_unique_id(killer));

    auto const restarted{ledger.record_spawn(EntityType::Turret, Team::Red, true)};
    EXPECT_EQ(restarted, killer);
    EXPECT_EQ(ledger.get_history_index(restarted), 0);
    EXPECT_EQ(ledger.count_alive(), 1);
}
} // namespace ioj::sim::tests
