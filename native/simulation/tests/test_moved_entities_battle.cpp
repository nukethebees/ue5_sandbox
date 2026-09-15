#include <ioj/sim/entity_registry.h>
#include <ioj/sim/level_sim.h>
#include <map>
#include <set>
#include "support/simulation_test_support.h"

namespace ioj::sim::tests {

namespace {
struct TransformSnapshot {
    float x{};
    float y{};
    float z{};
    float pitch{};
    float yaw{};
    float roll{};

    auto operator==(TransformSnapshot const&) const -> bool = default;
};

auto get_transform_snapshot(EntityRegistry::EntityData const& data, std::int32_t const index)
    -> TransformSnapshot {
    return {
        .x = data.locations.xs[index],
        .y = data.locations.ys[index],
        .z = data.locations.zs[index],
        .pitch = data.rotations.pitches[index],
        .yaw = data.rotations.yaws[index],
        .roll = data.rotations.rolls[index],
    };
}

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
    data.level_events.initial_spawns.capital_spawns.target_entity_indices[0] = second;

    auto const entity_type_count{data.entity_bounds.num()};
    for (std::int32_t type_index{}; type_index < entity_type_count; ++type_index) {
        data.entity_bounds.half_extent_xs[type_index] = 10.f;
        data.entity_bounds.half_extent_ys[type_index] = 10.f;
        data.entity_bounds.half_extent_zs[type_index] = 10.f;
    }
    return data;
}
}

TEST(MovedEntitiesBattle, HeadlessBattlePreservesUniquePerTickMovementAcrossLongBattle) {

    LevelSim simulation{make_long_running_battle()};
    simulation.finish_initialisation();

    std::map<RegistryEntityHandle, TransformSnapshot> previous_transforms;
    auto capture_current_transforms = [&](EntityRegistry const& registry) {
        previous_transforms.clear();
        auto const& data{registry.get_entity_data()};
        auto const generations{registry.get_generations()};
        auto const count{static_cast<std::int32_t>(generations.size())};
        for (std::int32_t index{}; index < count; ++index) {
            RegistryEntityHandle const handle{index, generations[index]};
            previous_transforms.emplace(handle, get_transform_snapshot(data, index));
        }
    };
    capture_current_transforms(simulation.get_entity_registry());

    std::int32_t observed_ticks{};
    std::int32_t observed_moved_fighters{};
    bool observed_empty_tick{};
    simulation.on_end_tick = [&](LevelSim& level) {
        auto const& registry{level.get_entity_registry()};
        auto const& data{registry.get_entity_data()};
        auto const generations{registry.get_generations()};
        auto const count{static_cast<std::int32_t>(generations.size())};

        std::set<RegistryEntityHandle> expected_moved;
        for (std::int32_t index{}; index < count; ++index) {
            RegistryEntityHandle const handle{index, generations[index]};
            auto const current{get_transform_snapshot(data, index)};
            if (auto const previous{previous_transforms.find(handle)};
                previous != previous_transforms.end() && previous->second != current) {
                expected_moved.insert(handle);
            }
        }

        auto const moved{registry.get_moved_entities_this_tick()};
        std::set<RegistryEntityHandle> unique_moved;
        for (auto const handle : moved) {
            tests::expect_true(registry.is_valid_handle(handle),
                               "Moved handle remains valid after registry end_tick");
            tests::expect_false(std::ranges::contains(unique_moved, handle),
                                "Moved handle occurs only once in its tick");
            unique_moved.insert(handle);
            tests::expect_true(std::ranges::contains(expected_moved, handle),
                               "Moved handle changed from the prior committed transform");
            if (registry.get_entity_type(handle) == EntityType::Fighter) {
                ++observed_moved_fighters;
            }
        }
        tests::expect_equal(static_cast<std::int32_t>(moved.size()),
                            static_cast<std::int32_t>(expected_moved.size()),
                            "Movement list exactly matches this tick's transform changes");
        observed_empty_tick = observed_empty_tick || moved.empty();
        ++observed_ticks;
        capture_current_transforms(registry);
    };

    simulation.start();
    auto const tick_period{simulation.get_clock().get_tick_period()};
    constexpr std::int32_t tick_count{3000};
    for (std::int32_t tick{}; tick < tick_count; ++tick) {
        simulation.advance(tick_period);
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

} // namespace tests
