#include "simulation_fixture.h"

#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/sim/levels/simulation_settings.h>

#include <filesystem>
#include <stdexcept>

namespace ioj::sim::tests {
auto make_fixture() -> SimulationFixture {
    static auto const settings{[] {
        auto const script_root{std::filesystem::path{IOJ_PROJECT_SOURCE_DIR} / "LevelScripts"};
        levels::authoring::DefinitionReader reader{script_root};
        auto const base{reader.read_settings_file(script_root / "Settings" / "base.scm")};
        if (!base) {
            throw std::runtime_error{levels::format_diagnostics(base.error())};
        }

        SimulationSettings resolved{};
        auto const applied{apply_settings(*base, resolved)};
        if (!applied) {
            throw std::runtime_error{levels::format_diagnostics(applied.error())};
        }

        auto const valid{validate_settings(resolved)};
        if (!valid) {
            throw std::runtime_error{levels::format_diagnostics(valid.error())};
        }
        return resolved;
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
