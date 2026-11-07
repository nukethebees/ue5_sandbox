#include "support/simulation_test_support.h"
#include <ioj/column_math.h>
#include <ioj/rotator_math.h>
#include <ioj/sim/levels/level_sim.h>

#include <map>
#include <set>

namespace ioj::sim::tests {

namespace {
struct FighterTransformSnapshot {
    Vector3f location{};
    Rotator3f rotation{};

    auto operator==(FighterTransformSnapshot const& other) const -> bool {
        return location == other.location && rotation.pitch == other.rotation.pitch &&
               rotation.yaw == other.rotation.yaw && rotation.roll == other.rotation.roll;
    }
};

auto make_long_running_battle() -> SimulationScenario {
    SimulationScenario data;
    data.runtime.clock_settings.tick_rate = ioj::Frequency::make(60.0);
    data.runtime.collision.grid_geometry = {{96, 96, 32}, {{2000.f, 2000.f, 5000.f}}};
    data.runtime.lasers.n_preallocated_instances = ::ioj::NonNegative<std::int32_t>::make(256);
    data.runtime.fighters.speed = 3000.f;
    data.runtime.fighters.laser.damage = 0;
    data.runtime.capital_ships.fighter_spawn_slots = 8;
    for (std::int32_t slot{}; slot < data.runtime.capital_ships.fighter_spawn_slots; ++slot) {
        auto const y{static_cast<float>(slot - 4) * 300.f};
        data.runtime.capital_ships.fighter_spawn_slots_relative_transforms.push_back(
            {.location = {1000.0, y, 0.0}});
    }

    add_capital_spawn(data,
                      {{-10000.f, 0.f, 0.f}},
                      Team::Green,
                      0.f,
                      10000.f,
                      ioj::settings::PositiveHealth::make(
                          ioj::sim::Health{std::numeric_limits<std::int32_t>::max()}));
    add_capital_spawn(data,
                      {{10000.f, 0.f, 0.f}},
                      Team::White,
                      0.f,
                      10000.f,
                      ioj::settings::PositiveHealth::make(
                          ioj::sim::Health{std::numeric_limits<std::int32_t>::max()}));

    for (auto const type : EnumTraits<EntityType>::values) {
        data.runtime.collision.entity_bounds.set_half_extents(type, {{10.f, 10.f, 10.f}});
    }
    return data;
}
}

TEST(MovedEntitiesBattle, HeadlessBattleTracksFighterTransformsAcrossLongBattle) {

    WorldlessSimulationTest harness{make_long_running_battle().compile()};
    harness.finish_initialisation();
    auto& simulation{harness.simulation()};

    std::map<EntityId, FighterTransformSnapshot> previous_transforms;
    auto capture_current_transforms = [&](fighters::Sim const& fighters) {
        previous_transforms.clear();
        auto const entities{fighters.entities()};
        auto const entity_count{entities.num()};
        auto const entity_ids{entities.entity_ids()};
        auto const locations{entities.view_locations().resolve()};
        auto const aim_directions{entities.view_aim_directions().resolve()};
        for (std::uint32_t index{}; index < entity_count; ++index) {
            previous_transforms.emplace(
                entity_ids[index],
                FighterTransformSnapshot{
                    .location = vector_at(locations, index),
                    .rotation = direction_to_rotation(vector_at(aim_directions, index))});
        }
    };
    capture_current_transforms(simulation.fighters());

    std::int32_t observed_ticks{};
    std::int32_t observed_moved_fighters{};
    bool observed_empty_tick{};
    harness.on_end_tick = [&](LevelSim& level) {
        auto const entities{level.fighters().entities()};
        std::set<EntityId> unique_entities;
        std::int32_t moved_count{};
        auto const entity_count{entities.num()};
        auto const entity_ids{entities.entity_ids()};
        auto const locations{entities.view_locations().resolve()};
        auto const aim_directions{entities.view_aim_directions().resolve()};
        for (std::uint32_t index{}; index < entity_count; ++index) {
            auto const id{entity_ids[index]};
            EXPECT_TRUE(unique_entities.insert(id).second)
                << "Each live fighter has a unique entity ID";
            auto const current{FighterTransformSnapshot{
                .location = vector_at(locations, index),
                .rotation = direction_to_rotation(vector_at(aim_directions, index))}};
            auto const previous{previous_transforms.find(id)};
            if (previous != previous_transforms.end() && previous->second != current) {
                ++moved_count;
            }
        }

        observed_empty_tick = observed_empty_tick || moved_count == 0;
        observed_moved_fighters += moved_count;
        ++observed_ticks;
        capture_current_transforms(level.fighters());
    };

    simulation.start();
    auto const tick_period{simulation.clock().tick_period()};
    constexpr std::int32_t tick_count{3000};
    for (std::int32_t tick{}; tick < tick_count; ++tick) {
        harness.advance(tick_period);
    }
    simulation.pause();
    simulation.advance(0.0);

    EXPECT_EQ(observed_ticks, tick_count) << "Every requested battle tick was observed";
    EXPECT_TRUE(observed_empty_tick) << "Battle includes an initially quiet movement tick";
    EXPECT_TRUE(observed_moved_fighters > 100) << "Battle exercises fighter movement";
    EXPECT_EQ(simulation.entity_ledger().issued_counts()[EntityType::Fighter], 16)
        << "Battle spawns exactly one fighter wave";
    EXPECT_EQ(simulation.fighters().num_instances(), 16)
        << "Zero-damage battle preserves every fighter";
}

} // namespace ioj::sim::tests
