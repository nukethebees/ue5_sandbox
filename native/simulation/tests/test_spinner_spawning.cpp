#include <ioj/sim/agent_accessor.h>
#include <ioj/sim/combat_events.h>
#include <ioj/sim/entity_ledger.h>
#include <ioj/sim/lasers/sim.h>
#include <ioj/sim/memory/game_memory.h>
#include <ioj/sim/sim_clock.h>
#include <ioj/sim/spatial_query_manager.h>
#include <ioj/sim/spinners/sim.h>
#include <sandbox/core/tick_countdown.h>
#include "support/simulation_test_support.h"

namespace ioj::sim::spinners {
struct SpinnerSpawnTestAccess {
    static void spawn(Sim& simulation,
                      Vectors3f::ConstView locations,
                      std::span<float const> yaws,
                      std::span<std::int32_t const> fire_points) {
        simulation.spawn_instances(
            locations,
            std::span<float const>{
                yaws.data(), static_cast<std::size_t>(static_cast<std::int32_t>(yaws.size()))},
            std::span<std::int32_t const>{
                fire_points.data(),
                static_cast<std::size_t>(static_cast<std::int32_t>(fire_points.size()))});
    }
    static auto entities(Sim& simulation) -> Sim::EntityStorage& { return simulation.entities; }
    static void tick_cooldowns(Sim& simulation) { simulation.prepare_tick(0.f); }
    static void set_cooldown(Sim& simulation, std::int16_t const ticks) {
        simulation.cooldown_restart_ticks_ = ticks;
    }
    static void rotate(Sim& simulation, float const yaw_delta, ml::FrameScratch& scratch) {
        simulation.planned_yaw_delta_ = yaw_delta;
        simulation.apply_movement(scratch);
    }
    static auto cooldowns(Sim& simulation) -> ml::TickCountdownView<std::int16_t> {
        return {simulation.entities.get_view().laser_cooldowns(),
                simulation.cooldown_restart_ticks_};
    }
};
}

namespace ioj::sim::tests {

TEST(SpinnerSpawning, RepeatedAppendsPreserveRowsAndCooldowns) {

    using Access = spinners::SpinnerSpawnTestAccess;
    SimClock clock;
    EntityLedger ledger;
    CombatEvents combat_events{ledger};
    AgentIndexes indexes{clock};
    HealthTable health_table;
    AgentAccessor agents{indexes, health_table};
    SpatialQueryManager queries{agents};
    GameMemory game_memory{{.root_capacity_bytes = 2 * 1024 * 1024}};
    auto frame_block{
        game_memory.acquire_block(1024 * 1024, ml::FrameMemoryResource::backing_alignment)};
    ml::FrameMemoryResource frame_memory{
        std::span<std::byte>{frame_block.data(), frame_block.size_bytes()}};
    lasers::Sim lasers{clock, combat_events, queries};
    spinners::Sim simulation{clock, ledger, lasers};
    auto& entity_storage{Access::entities(simulation)};
    Access::set_cooldown(simulation, 23);

    Vectors3f locations;
    locations.add_defaulted(3);
    locations.set(0, Vector3f{{1.f, 2.f, 3.f}});
    locations.set(1, Vector3f{{4.f, 5.f, 6.f}});
    locations.set(2, Vector3f{{7.f, 8.f, 9.f}});
    std::vector<float> const yaws{10.f, 20.f, 30.f};
    std::vector<std::int32_t> const fire_points{2, 0, 1};
    Access::spawn(simulation,
                  locations.left(1).get_const_view(),
                  std::span{yaws}.first(1),
                  std::span{fire_points}.first(1));
    auto const first_id{entity_storage.get_const_view().entity_ids()[0]};
    Access::cooldowns(simulation).restart_counter(0);
    Access::tick_cooldowns(simulation);
    auto const first_cooldown{entity_storage.get_const_view().laser_cooldowns()[0]};

    Access::spawn(simulation,
                  locations.right(2).get_const_view(),
                  std::span{yaws}.last(2),
                  std::span{fire_points}.last(2));
    auto const second_id{entity_storage.get_const_view().entity_ids()[1]};
    Access::cooldowns(simulation).restart_counter(1);
    Access::spawn(simulation, locations.get_const_view(), yaws, fire_points);
    Access::spawn(simulation,
                  locations.left(0).get_const_view(),
                  std::span{yaws}.first(0),
                  std::span{fire_points}.first(0));

    auto const entities{entity_storage.get_const_view().columns()};
    entities.validate_array_sizes();
    ASSERT_EQ((6), (simulation.get_num_instances()));
    ASSERT_TRUE((first_id == entities.entity_ids[0]));
    ASSERT_TRUE((second_id == entities.entity_ids[1]));
    ASSERT_EQ((first_cooldown), (entities.laser_cooldowns[0]));
    ASSERT_EQ((std::int16_t{23}), (entities.laser_cooldowns[1]));
    auto const count{entities.num()};
    for (std::int32_t i{}; i < count; ++i) {
        auto const source_index{i % 3};
        ASSERT_TRUE((yaws[source_index] == entities.yaws[i]));
        ASSERT_EQ((fire_points[source_index]), (entities.next_fire_point_indices[i]));
        ASSERT_TRUE((locations.xs[source_index] == entities.locations.xs[i]));
        ASSERT_TRUE((locations.ys[source_index] == entities.locations.ys[i]));
        ASSERT_TRUE((locations.zs[source_index] == entities.locations.zs[i]));
        ASSERT_TRUE((ledger.is_valid_unique_id(entities.entity_ids[i])));
        for (std::int32_t j{}; j < i; ++j) {
            ASSERT_TRUE((entities.entity_ids[i] != entities.entity_ids[j]));
        }
        if (i >= 2) {
            ASSERT_EQ((std::int16_t{0}), (entities.laser_cooldowns[i]));
        }
    }
    Access::cooldowns(simulation).restart_counter(5);
    ASSERT_EQ((std::int16_t{23}), (entities.laser_cooldowns[5]));

    {
        ml::FrameScratchScope scratch_scope{frame_memory};
        Access::rotate(simulation, 5.f, scratch_scope.scratch());
    }
    auto const rotated_entities{entity_storage.get_const_view().columns()};
    for (std::int32_t i{}; i < count; ++i) {
        ASSERT_EQ((yaws[i % 3] + 5.f), (rotated_entities.yaws[i]));
    }
}

} // namespace tests
