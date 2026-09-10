#include <SpaceGameSimulation/entities/TestEntityRegistry.h>
#include <SpaceGameSimulation/simulation/LevelSimulation.h>

#include <CQTest.h>

namespace {
struct FTransformSnapshot {
    float x{};
    float y{};
    float z{};
    float pitch{};
    float yaw{};
    float roll{};

    auto operator==(FTransformSnapshot const&) const -> bool = default;
};

auto get_transform_snapshot(FTestEntityRegistry::EntityData const& data, int32 const index)
    -> FTransformSnapshot {
    return {
        .x = data.locations.xs[index],
        .y = data.locations.ys[index],
        .z = data.locations.zs[index],
        .pitch = data.rotations.pitches[index],
        .yaw = data.rotations.yaws[index],
        .roll = data.rotations.rolls[index],
    };
}

auto make_long_running_battle() -> FLevelSimulationInitData {
    FLevelSimulationInitData data;
    data.clock_settings.tick_rate = 60.0;
    data.grid_dimensions = {96, 96, 32};
    data.cell_size = {2000.f, 2000.f, 5000.f};
    data.lasers.n_preallocated_instances = 256;
    data.fighters.speed = 3000.f;
    data.fighters.laser.damage = 0;
    data.capital_ships.fighter_spawn_slots = 8;
    for (int32 slot{}; slot < data.capital_ships.fighter_spawn_slots; ++slot) {
        auto const y{static_cast<float>(slot - 4) * 300.f};
        data.capital_ships.fighter_spawn_slots_relative_transforms.Emplace(
            FTransform{FVector{1000.f, y, 0.f}});
    }

    data.capital_spawns.add_defaulted(2);
    data.capital_spawns.locations.xs = {-10000.f, 10000.f};
    data.capital_spawns.teams = {ETestTeam::Green, ETestTeam::White};
    data.capital_spawns.healths = {MAX_int32, MAX_int32};
    data.capital_spawns.initial_spawn_delays = {0.f, 0.f};
    data.capital_spawns.spawn_cooldowns = {10000.f, 10000.f};
    data.capital_target_spawn_indices = {1, 0};

    auto const entity_type_count{ml::ioj::FEntityAABBs::num()};
    for (int32 type_index{}; type_index < entity_type_count; ++type_index) {
        data.entity_bounds.half_extent_xs[type_index] = 10.f;
        data.entity_bounds.half_extent_ys[type_index] = 10.f;
        data.entity_bounds.half_extent_zs[type_index] = 10.f;
    }
    return data;
}
}

TEST_CLASS(MovedEntitiesBattle, "Sandbox.UnitTests")
{
    TEST_METHOD(HeadlessBattlePreservesUniquePerTickMovementAcrossLongBattle)
    {
        FLevelSimulation simulation{make_long_running_battle()};
        simulation.finish_initialisation();

        TMap<FRegistryEntityHandle, FTransformSnapshot> previous_transforms;
        auto capture_current_transforms = [&](FTestEntityRegistry const& registry) {
            previous_transforms.Reset();
            auto const& data{registry.get_entity_data()};
            auto const generations{registry.get_generations()};
            auto const count{generations.Num()};
            for (int32 index{}; index < count; ++index) {
                FRegistryEntityHandle const handle{index, generations[index]};
                previous_transforms.Add(handle, get_transform_snapshot(data, index));
            }
        };
        capture_current_transforms(simulation.get_entity_registry());

        int32 observed_ticks{};
        int32 observed_moved_fighters{};
        bool observed_empty_tick{};
        simulation.on_end_tick = [&](FLevelSimulation& level) {
            auto const& registry{level.get_entity_registry()};
            auto const& data{registry.get_entity_data()};
            auto const generations{registry.get_generations()};
            auto const count{generations.Num()};

            TSet<FRegistryEntityHandle> expected_moved;
            for (int32 index{}; index < count; ++index) {
                FRegistryEntityHandle const handle{index, generations[index]};
                auto const current{get_transform_snapshot(data, index)};
                if (auto const* previous{previous_transforms.Find(handle)};
                    previous && *previous != current) {
                    expected_moved.Add(handle);
                }
            }

            auto const moved{registry.get_moved_entities_this_tick()};
            TSet<FRegistryEntityHandle> unique_moved;
            for (auto const handle : moved) {
                TestRunner->TestTrue(TEXT("Moved handle remains valid after registry end_tick"),
                                     registry.is_valid_handle(handle));
                TestRunner->TestFalse(TEXT("Moved handle occurs only once in its tick"),
                                      unique_moved.Contains(handle));
                unique_moved.Add(handle);
                TestRunner->TestTrue(
                    TEXT("Moved handle changed from the prior committed transform"),
                    expected_moved.Contains(handle));
                if (registry.get_entity_type(handle) == ETestEntityType::CapitalShipFighter) {
                    ++observed_moved_fighters;
                }
            }
            TestRunner->TestEqual(
                TEXT("Movement list exactly matches this tick's transform changes"),
                moved.Num(),
                expected_moved.Num());
            observed_empty_tick = observed_empty_tick || moved.IsEmpty();
            ++observed_ticks;
            capture_current_transforms(registry);
        };

        simulation.start();
        auto const tick_period{simulation.get_clock().get_tick_period()};
        constexpr int32 tick_count{3000};
        for (int32 tick{}; tick < tick_count; ++tick) {
            simulation.advance(tick_period);
        }
        simulation.pause();

        TestRunner->TestEqual(
            TEXT("Every requested battle tick was observed"), observed_ticks, tick_count);
        TestRunner->TestTrue(TEXT("Battle includes an initially quiet movement tick"),
                             observed_empty_tick);
        TestRunner->TestTrue(TEXT("Battle exercises fighter movement"),
                             observed_moved_fighters > 100);
        TestRunner->TestEqual(TEXT("Battle spawns exactly one fighter wave"),
                              simulation.get_capital_ships().get_fighters_spawned(),
                              16);
        TestRunner->TestEqual(TEXT("Zero-damage battle preserves every fighter"),
                              simulation.get_capital_ship_fighters().get_num_instances(),
                              16);
    }
};
