#include "support/simulation_test_support.h"
#include <ioj/sim/column_math.h>
#include <ioj/sim/testing/level_sim_test_access.h>

#include <sandbox/core/vector_normalization.h>

namespace ioj::sim::tests::fighter_target_refresh {

auto make_world() -> LevelSimInitData {
    LevelSimInitData data;
    data.grid_geometry = {{16, 16, 4}, {{1000.f, 1000.f, 1000.f}}};
    data.lasers.n_preallocated_instances = 8;
    data.capital_ships.fighter_spawn_slots = 0;
    data.fighters.attack_engagement_threshold = 100.f;
    data.fighters.laser.max_distance = 1000.f;
    data.fighters.health = 100;
    data.participating_teams.add(Team::Green);
    data.participating_teams.add(Team::Red);
    add_capital_spawn(data, {{-1000.f, -1000.f, 0.f}}, Team::Green, -1, 60.f, 60.f, 100);
    add_capital_spawn(data, {{-2000.f, 0.f, 0.f}}, Team::Red, -1, 60.f, 60.f, 100);
    for (auto const type : ml::EnumTraits<EntityType>::values) {
        data.entity_bounds.set_half_extents(type, {{10.f, 10.f, 10.f}});
    }
    data.entity_bounds.set_half_extents(EntityType::CapitalShip, {{100.f, 100.f, 100.f}});
    return data;
}

void expect_vector(Vector3f const actual, Vector3f const expected) {
    EXPECT_FLOAT_EQ(actual.X, expected.X);
    EXPECT_FLOAT_EQ(actual.Y, expected.Y);
    EXPECT_FLOAT_EQ(actual.Z, expected.Z);
}

class FighterTargetRefresh : public ::testing::Test {
  protected:
    void SetUp() override {
        simulation.finish_initialisation();
        SingleAllocationFighterSpawnQueue spawns;
        spawns.add_defaulted(1);
        auto const spawn{spawns.get_view()};
        for (std::int32_t i{}; i < 2; ++i) {
            set_vector(spawn.view_locations(), 0, {{100.f + 1000.f * i, 100.f + 300.f * i, 0.f}});
            spawn.teams()[0] = i == 0 ? Team::Green : Team::Red;
            spawn.parents()[0] = simulation.get_capital_ships().get_id(i);
            LevelSimTestAccess::commit_fighter_spawns(simulation, spawns.get_const_view());
        }
        fighter = simulation.get_fighters().get_entity_ids()[0];
        target = simulation.get_fighters().get_entity_ids()[1];
        LevelSimTestAccess::set_fighter_kinematics(
            simulation, target, {{1100.f, 400.f, 0.f}}, {{0.f, 120.f, 0.f}});
        LevelSimTestAccess::set_fighter_target(simulation, target, fighter, 5);
    }

    auto think() -> std::uint64_t {
        auto const before{memory.get_stats().current_root_claim_count};
        ml::FrameScratchScope scope{memory};
        LevelSimTestAccess::think_fighters(simulation, scope.scratch());
        return memory.get_stats().current_root_claim_count - before;
    }

    auto single_refresh_and_plan() -> std::uint64_t {
        auto const before{memory.get_stats().current_root_claim_count};
        ml::FrameScratchScope scope{memory};
        LevelSimTestAccess::refresh_fighter_targets_and_plan(simulation, scope.scratch());
        return memory.get_stats().current_root_claim_count - before;
    }

    void expect_target_state(EntityUniqueId const expected) {
        auto const entities{simulation.get_fighters().get_read_view().entities};
        auto const index{simulation.get_agent_indexes().find(fighter)};
        auto const state{simulation.get_agent_accessor().read_alive(expected)};
        ASSERT_TRUE(state);
        EXPECT_EQ(entities.target_ids()[index], expected);
        expect_vector(vector_at(entities.view_target_locations(), index), state->location);
        expect_vector(vector_at(entities.view_target_velocities(), index), state->velocity);
        EXPECT_FLOAT_EQ(
            entities.target_radii()[index],
            simulation.get_spatial_query_manager().get_entity_type_radius(expected.entity_type()));
        auto const offset{state->location - vector_at(entities.view_locations(), index)};
        auto const next_location{vector_at(entities.view_locations(), index) +
                                 vector_at(entities.view_movement_directions(), index) *
                                     entities.move_distances()[index]};
        EXPECT_FLOAT_EQ(entities.target_distances()[index],
                        HMM_LenV3(state->location - next_location));
        EXPECT_FLOAT_EQ(entities.target_distance_sq()[index],
                        HMM_LenSqrV3(state->location - next_location));
        auto const intercept{state->location + state->velocity * entities.intercept_times()[index]};
        expect_vector(vector_at(entities.view_desired_aiming_directions(), index),
                      ml::native_math::safe_normal(
                          intercept - vector_at(entities.view_locations(), index), 1.e-8f));
        auto const direction{ml::native_math::safe_normal(offset, 1.e-8f)};
        expect_vector(vector_at(entities.view_desired_move_locations(), index),
                      state->location - direction * (1000.f * AttackDistanceBand{}.desired_ratio));
    }

    void kill_target() {
        DirectDamageEvents damage;
        damage.add_uninitialised(1);
        damage.damaged_entities[0] = target;
        damage.damage_amounts[0] = 100;
        LevelSimTestAccess::queue_direct_damage_events(simulation, damage.get_const_view());
        ml::FrameScratchScope scope{memory};
        LevelSimTestAccess::resolve_fighter_damage(simulation, scope.scratch());
    }

    void expect_cleared_target() {
        auto const entities{simulation.get_fighters().get_read_view().entities};
        auto const index{simulation.get_agent_indexes().find(fighter)};
        EXPECT_FALSE(entities.target_ids()[index].is_valid());
        expect_vector(vector_at(entities.view_target_locations(), index), {});
        expect_vector(vector_at(entities.view_target_velocities(), index), {});
        EXPECT_FLOAT_EQ(entities.target_radii()[index], 0.f);
        auto const next_location{vector_at(entities.view_locations(), index) +
                                 vector_at(entities.view_movement_directions(), index) *
                                     entities.move_distances()[index]};
        EXPECT_FLOAT_EQ(entities.target_distances()[index], HMM_LenV3(next_location));
        EXPECT_FLOAT_EQ(entities.target_distance_sq()[index], HMM_LenSqrV3(next_location));
        expect_vector(
            vector_at(entities.view_desired_aiming_directions(), index),
            ml::native_math::safe_normal(-vector_at(entities.view_locations(), index), 1.e-8f));
    }

    LevelSim simulation{make_world()};
    EntityUniqueId fighter;
    EntityUniqueId target;
    alignas(ml::FrameMemoryResource::backing_alignment) std::array<std::byte, 64 * 1024> backing{};
    ml::FrameMemoryResource memory{backing};
};

TEST_F(FighterTargetRefresh, UnchangedTargetUsesOneRefreshAndReadsCurrentKinematics) {
    LevelSimTestAccess::set_fighter_target(simulation, fighter, target, 5);
    single_refresh_and_plan();
    LevelSimTestAccess::set_fighter_kinematics(
        simulation, target, {{1300.f, 500.f, 0.f}}, {{0.f, 180.f, 0.f}});
    LevelSimTestAccess::set_fighter_target(simulation, fighter, target, 5);
    auto const claims{think()};
    expect_target_state(target);
    EXPECT_EQ(claims, single_refresh_and_plan());
    EXPECT_EQ(simulation.get_fighters().get_read_view().entities.awareness_scan_countdowns()[0], 5);
}

TEST_F(FighterTargetRefresh, AwarenessSelectingSameIdUsesOneRefresh) {
    LevelSimTestAccess::set_fighter_target(simulation, fighter, target);
    auto const claims{think()};
    expect_target_state(target);
    EXPECT_EQ(claims, single_refresh_and_plan());
    EXPECT_GT(simulation.get_fighters().get_read_view().entities.awareness_scan_countdowns()[0], 0);
}

TEST_F(FighterTargetRefresh, AcquisitionRefreshesBeforePlanning) {
    LevelSimTestAccess::set_fighter_target(simulation, fighter, {});
    auto const claims{think()};
    expect_target_state(target);
    EXPECT_GT(claims, single_refresh_and_plan());
}

TEST_F(FighterTargetRefresh, ReplacementRefreshesEveryGatheredFieldBeforePlanning) {
    auto const old_target{simulation.get_capital_ships().get_id(1)};
    LevelSimTestAccess::set_fighter_target(simulation, fighter, old_target);
    single_refresh_and_plan();
    expect_target_state(old_target);
    LevelSimTestAccess::set_fighter_target(simulation, fighter, old_target);
    auto const claims{think()};
    expect_target_state(target);
    EXPECT_GT(claims, single_refresh_and_plan());
}

TEST_F(FighterTargetRefresh, DeadTargetIsClearedByInitialRefreshBeforeRemoval) {
    LevelSimTestAccess::set_fighter_target(simulation, fighter, target, 5);
    single_refresh_and_plan();
    kill_target();
    ASSERT_GE(simulation.get_agent_indexes().find(target), 0);
    ASSERT_FALSE(simulation.get_agent_accessor().read_alive(target));
    LevelSimTestAccess::set_fighter_target(simulation, fighter, target, 5);
    auto const claims{think()};
    expect_cleared_target();
    EXPECT_EQ(claims, single_refresh_and_plan());
}

TEST_F(FighterTargetRefresh, MissingTargetIsClearedByInitialRefresh) {
    LevelSimTestAccess::set_fighter_target(simulation, fighter, target, 5);
    single_refresh_and_plan();
    kill_target();
    LevelSimTestAccess::remove_dead_fighters(simulation);
    ASSERT_LT(simulation.get_agent_indexes().find(target), 0);
    LevelSimTestAccess::set_fighter_target(simulation, fighter, target, 5);
    auto const claims{think()};
    expect_cleared_target();
    EXPECT_EQ(claims, single_refresh_and_plan());
}

TEST_F(FighterTargetRefresh, MissingTargetCanBeReacquiredInSameThinkingPhase) {
    auto const missing{
        EntityUniqueId{entity_identity_offset(EntityType::Fighter, 999), EntityType::Fighter}};
    LevelSimTestAccess::set_fighter_target(simulation, fighter, missing);
    auto const claims{think()};
    expect_target_state(target);
    EXPECT_GT(claims, single_refresh_and_plan());
}

} // namespace ioj::sim::tests::fighter_target_refresh
