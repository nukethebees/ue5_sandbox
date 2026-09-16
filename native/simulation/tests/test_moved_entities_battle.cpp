#include <ioj/sim/level_sim.h>
#include <ioj/sim/rotator_math.h>
#include <map>
#include <set>
#include "support/simulation_test_support.h"

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

auto make_long_running_battle() -> LevelSimInitData {
    LevelSimInitData data;
    data.clock_settings.tick_rate = 60.0;
    data.grid_dimensions = {96, 96, 32};
    data.cell_size = {{2000.f, 2000.f, 5000.f}};
    data.lasers.n_preallocated_instances = 256;
    data.fighters.speed = 3000.f;
    data.fighters.laser.damage = 0;
    data.capital_ships.fighter_spawn_slots = 8;
    for (std::int32_t slot{}; slot < data.capital_ships.fighter_spawn_slots; ++slot) {
        auto const y{static_cast<float>(slot - 4) * 300.f};
        data.capital_ships.fighter_spawn_slots_relative_transforms.push_back(
            {.location = {1000.0, y, 0.0}});
    }

    auto const first{add_capital_spawn(data,
                                       {{-10000.f, 0.f, 0.f}},
                                       Team::Green,
                                       -1,
                                       0.f,
                                       10000.f,
                                       std::numeric_limits<std::int32_t>::max())};
    auto const second{add_capital_spawn(data,
                                        {{10000.f, 0.f, 0.f}},
                                        Team::White,
                                        first,
                                        0.f,
                                        10000.f,
                                        std::numeric_limits<std::int32_t>::max())};
    data.level_events.initial_spawns.capital_spawns.get_view().target_entity_indices()[0] = second;

    auto const entity_type_count{data.entity_bounds.num()};
    for (std::int32_t type_index{}; type_index < entity_type_count; ++type_index) {
        data.entity_bounds.set_half_extents(type_index, {{10.f, 10.f, 10.f}});
    }
    return data;
}
}

TEST(MovedEntitiesBattle, HeadlessBattlePreservesUniquePerTickMovementAcrossLongBattle) {

    WorldlessSimulationTest harness{make_long_running_battle()};
    harness.finish_initialisation();
    auto& simulation{harness.get_simulation()};

    std::map<EntityUniqueId, FighterTransformSnapshot> previous_transforms;
    auto capture_current_transforms = [&](fighters::Sim const& fighters) {
        previous_transforms.clear();
        auto const entities{fighters.get_read_view().entities};
        for (std::int32_t index{}; index < entities.num(); ++index) {
            previous_transforms.emplace(
                entities.entity_ids[index],
                FighterTransformSnapshot{
                    .location = entities.locations[index],
                    .rotation = direction_to_rotation(entities.aim_directions[index])});
        }
    };
    capture_current_transforms(simulation.get_fighters());

    std::int32_t observed_ticks{};
    std::int32_t observed_moved_fighters{};
    bool observed_empty_tick{};
    harness.on_end_tick = [&](LevelSim& level) {
        auto const entities{level.get_fighters().get_read_view().entities};
        std::set<EntityUniqueId> expected_moved;
        for (std::int32_t index{}; index < entities.num(); ++index) {
            auto const id{entities.entity_ids[index]};
            auto const current{FighterTransformSnapshot{
                .location = entities.locations[index],
                .rotation = direction_to_rotation(entities.aim_directions[index])}};
            auto const previous{previous_transforms.find(id)};
            if (previous != previous_transforms.end() && previous->second != current) {
                expected_moved.insert(id);
            }
        }

        auto const moved{level.get_fighters().get_collision_dirty_entities()};
        std::set<EntityUniqueId> const unique_moved{moved.begin(), moved.end()};
        for (auto const id : unique_moved) {
            if (!previous_transforms.contains(id)) {
                expected_moved.insert(id);
            }
        }
        tests::expect_equal(static_cast<std::int32_t>(moved.size()),
                            static_cast<std::int32_t>(unique_moved.size()),
                            "Moved ID occurs only once in its tick");
        tests::expect_true(unique_moved == expected_moved,
                           "Owner collision dirtiness exactly matches fighter movement");
        observed_empty_tick = observed_empty_tick || moved.empty();
        observed_moved_fighters += static_cast<std::int32_t>(moved.size());
        ++observed_ticks;
        capture_current_transforms(level.get_fighters());
    };

    simulation.start();
    auto const tick_period{simulation.get_clock().get_tick_period()};
    constexpr std::int32_t tick_count{3000};
    for (std::int32_t tick{}; tick < tick_count; ++tick) {
        harness.advance(tick_period);
    }
    simulation.pause();

    tests::expect_equal(observed_ticks, tick_count, "Every requested battle tick was observed");
    tests::expect_true(observed_empty_tick, "Battle includes an initially quiet movement tick");
    tests::expect_true(observed_moved_fighters > 100, "Battle exercises fighter movement");
    tests::expect_equal(simulation.get_capital_ships().get_fighters_spawned(),
                        16,
                        "Battle spawns exactly one fighter wave");
    tests::expect_equal(simulation.get_fighters().get_num_instances(),
                        16,
                        "Zero-damage battle preserves every fighter");
}

} // namespace ioj::sim::tests
