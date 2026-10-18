#include "simulation_fixture.h"

#include <ioj/s7/simulation_settings_reader.h>
#include <ioj/sim/levels/simulation_settings.h>

#include <filesystem>
#include <stdexcept>

namespace ioj::sim::tests {
auto make_fixture() -> SimulationFixture {
    static auto const settings{[] {
        auto const script_root{std::filesystem::path{IOJ_PROJECT_SOURCE_DIR} / "LevelScripts"};
        auto const loaded{
            s7::read_simulation_settings(script_root / "Settings" / "base.scm", script_root)};
        if (!loaded) {
            throw std::runtime_error{levels::format_diagnostics(loaded.error())};
        }
        return *loaded;
    }()};

    auto fixture{make_reference_level_simulation_data()};
    apply_settings(settings, fixture.data);
    apply_settings(settings, fixture.player);
    fixture.data.lasers.n_preallocated_instances = 256;
    fixture.data.grid_geometry.dimensions = {64, 64, 5};
    fixture.data.grid_geometry.cell_dimensions = {31250.f, 31250.f, 20000.f};
    fixture.data.frame_memory_capacity_bytes = 4 * 1024 * 1024;
    return fixture;
}
} // namespace ioj::sim::tests
