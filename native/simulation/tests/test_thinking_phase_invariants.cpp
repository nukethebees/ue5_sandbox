#include "support/simulation_test_support.h"
#include <ioj/sim/testing/level_sim_test_access.h>

#ifndef NDEBUG
namespace ioj::sim::tests::thinking_invariants {
auto make_data(Health const health = 100) -> LevelSimInitData {
    LevelSimInitData data;
    data.capital_ships.fighter_spawn_slots = 0;
    for (auto const type : ml::EnumTraits<EntityType>::values) {
        data.entity_bounds.set_half_extents(type, {{10.f, 10.f, 10.f}});
    }
    add_capital_spawn(
        data, {{-1000.f, 0.f, 0.f}}, Team::Blue, invalid_level_entity_index, 60.f, 60.f, health);
    add_capital_spawn(
        data, {{1000.f, 0.f, 0.f}}, Team::Red, invalid_level_entity_index, 60.f, 60.f, health);
    return data;
}

TEST(ThinkingPhaseInvariants, AcceptsLiveGridAndUnchangedState) {
    LevelSim simulation{make_data()};
    simulation.finish_initialisation();
    auto const state{LevelSimTestAccess::capture_thinking_phase_state(simulation)};

    EXPECT_TRUE(LevelSimTestAccess::check_thinking_entry_invariants(simulation, state));
    EXPECT_TRUE(LevelSimTestAccess::check_thinking_phase_invariants(simulation, state));
}

TEST(ThinkingPhaseInvariants, RejectsDeadEntitiesStillInGrid) {
    LevelSim simulation{make_data()};
    simulation.finish_initialisation();
    LevelSimTestAccess::set_capital_health(simulation, 0, 0);
    auto const state{LevelSimTestAccess::capture_thinking_phase_state(simulation)};

    EXPECT_FALSE(LevelSimTestAccess::check_thinking_entry_invariants(simulation, state));
}

TEST(ThinkingPhaseInvariants, RejectsLiveEntitiesMissingFromGrid) {
    LevelSim simulation{make_data(0)};
    simulation.finish_initialisation();
    LevelSimTestAccess::set_capital_health(simulation, 0, 100);
    auto const state{LevelSimTestAccess::capture_thinking_phase_state(simulation)};

    EXPECT_FALSE(LevelSimTestAccess::check_thinking_entry_invariants(simulation, state));
}

TEST(ThinkingPhaseInvariants, RejectsHealthChangesEvenWhenEntityRemainsAlive) {
    LevelSim simulation{make_data()};
    simulation.finish_initialisation();
    auto const state{LevelSimTestAccess::capture_thinking_phase_state(simulation)};
    LevelSimTestAccess::set_capital_health(simulation, 0, 99);

    EXPECT_FALSE(LevelSimTestAccess::check_thinking_phase_invariants(simulation, state));
}

TEST(ThinkingPhaseInvariants, RejectsTeamAndTransformWritesThroughOwnerViews) {
    LevelSim simulation{make_data()};
    simulation.finish_initialisation();
    auto const state{LevelSimTestAccess::capture_thinking_phase_state(simulation)};
    auto const entities{LevelSimTestAccess::capital_entities(simulation)};

    entities.teams()[0] = Team::Red;
    EXPECT_FALSE(LevelSimTestAccess::check_thinking_phase_invariants(simulation, state));
    entities.teams()[0] = Team::Blue;
    EXPECT_TRUE(LevelSimTestAccess::check_thinking_phase_invariants(simulation, state));

    entities.view_locations().xs()[0] += 1.f;
    EXPECT_FALSE(LevelSimTestAccess::check_thinking_phase_invariants(simulation, state));
    entities.view_locations().xs()[0] -= 1.f;
    entities.view_rotations().yaws()[0] += 1.f;
    EXPECT_FALSE(LevelSimTestAccess::check_thinking_phase_invariants(simulation, state));
}

TEST(ThinkingPhaseInvariants, PermitsDecisionStateChanges) {
    LevelSim simulation{make_data()};
    simulation.finish_initialisation();
    auto const state{LevelSimTestAccess::capture_thinking_phase_state(simulation)};
    auto const entities{LevelSimTestAccess::capital_entities(simulation)};
    entities.target_ids()[0] = entities.entity_ids()[1];

    EXPECT_TRUE(LevelSimTestAccess::check_thinking_phase_invariants(simulation, state));
}

TEST(ThinkingPhaseInvariants, RejectsPlayerTeamChanges) {
    auto data{make_data()};
    add_player_spawn(data, {});
    LevelSim simulation{std::move(data)};
    simulation.finish_initialisation();
    auto const state{LevelSimTestAccess::capture_thinking_phase_state(simulation)};
    auto& player{LevelSimTestAccess::player_simulation(simulation)};
    player.set_team(player.team == Team::Blue ? Team::Red : Team::Blue);

    EXPECT_FALSE(LevelSimTestAccess::check_thinking_phase_invariants(simulation, state));
}
} // namespace ioj::sim::tests::thinking_invariants
#endif
