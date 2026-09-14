#include <ioj/sim/spatial_query_manager.h>
#include "../support/simulation_test_support.h"

#include "test_spatial_query_manager.h"

namespace ioj::sim {
void run_worldless_spatial_query_line_of_sight(ioj::sim::tests::SimulationFixture const& config) {
    constexpr float distance{30000.f};
    std::vector<ioj::sim::Vector3f> const locations{{{0.f, distance, 0.f}},
                                                    {{0.f, -distance, 0.f}},
                                                    {{distance, 0.f, 0.f}},
                                                    {{-distance, 0.f, 0.f}}};
    auto data{ioj::sim::tests::make_simulation_data(config)};
    data.capital_ships.fighter_spawn_slots = 0;
    data.capital_ships.fighter_spawn_slots_relative_transforms.clear();
    for (auto const location : locations) {
        ioj::sim::tests::add_capital_spawn(data, location, ioj::sim::Team::White, -1, 999.f, 999.f);
    }
    ioj::sim::tests::WorldlessSimulationTest harness{std::move(data)};
    harness.finish_initialisation();
    auto const& capitals{harness.get_simulation().get_capital_ships()};
    std::vector<RegistryEntityHandle> expected{};
    for (std::int32_t i{}; i < static_cast<std::int32_t>(locations.size()); ++i) {
        expected.push_back(capitals.get_handle(i));
    }

    ioj::sim::Vectors3f starts;
    ioj::sim::Vectors3f ends;
    std::vector<RegistryEntityHandle> targets{};
    std::array<float, 3> const scales{0.5f, 1.f, 2.f};
    for (auto const scale : scales) {
        for (std::int32_t i{}; i < static_cast<std::int32_t>(locations.size()); ++i) {
            starts.add(ml::make_vector3f(0.f, 0.f, 0.f));
            ends.add(locations[i] * scale);
            targets.push_back(expected[i]);
        }
    }
    std::vector<RegistryEntityHandle> results{};
    results.resize(static_cast<std::size_t>(ends.num()));
    harness.get_simulation().get_spatial_query_manager().trace_line_of_sight(
        starts.get_const_view(), ends.get_const_view(), results);
    auto const count{static_cast<std::int32_t>(locations.size())};
    for (std::int32_t i{}; i < count; ++i) {
        ioj::sim::tests::expect_true(results[i].is_null(), "Half-distance trace misses", i);
        ioj::sim::tests::expect_equal(
            expected[i], results[i + count], "Ship trace resolves handle", i);
        ioj::sim::tests::expect_equal(
            expected[i], results[i + 2 * count], "Past-ship trace resolves handle", i);
    }

    std::vector<std::uint8_t> has_los{};
    has_los.resize(static_cast<std::size_t>(ends.num()));
    harness.get_simulation().get_spatial_query_manager().has_line_of_sight_to_targets(
        ml::make_vector3f(0.f, 0.f, 0.f), ends.get_const_view(), targets, has_los);
    for (std::int32_t i{}; i < static_cast<std::int32_t>(has_los.size()); ++i) {
        ioj::sim::tests::expect_equal(
            std::uint8_t{1}, has_los[i], "Clear or target hit has line of sight", i);
    }
    for (std::int32_t i{}; i < count; ++i) {
        auto const other{expected[(i + 1) % count]};
        targets[i + count] = other;
        targets[i + 2 * count] = other;
    }
    harness.get_simulation().get_spatial_query_manager().has_line_of_sight_to_targets(
        ml::make_vector3f(0.f, 0.f, 0.f), ends.get_const_view(), targets, has_los);
    for (std::int32_t i{}; i < count; ++i) {
        ioj::sim::tests::expect_equal(std::uint8_t{1}, has_los[i], "Clear line remains visible", i);
        ioj::sim::tests::expect_equal(
            std::uint8_t{0}, has_los[i + count], "Other target is blocked", i);
        ioj::sim::tests::expect_equal(
            std::uint8_t{0}, has_los[i + 2 * count], "Other target past hit is blocked", i);
    }
    ioj::sim::tests::expect_true(true, "Line-of-sight query batch completed");
}

}
