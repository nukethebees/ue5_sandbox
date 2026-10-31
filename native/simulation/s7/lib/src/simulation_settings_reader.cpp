#include <ioj/s7/simulation_settings_reader.h>

#include <ioj/levels/authoring/definition_reader.h>

namespace ioj::s7 {
auto read_simulation_settings(std::filesystem::path const& path,
                              std::filesystem::path const& library_root)
    -> SimulationSettingsReadResult {
    auto source{levels::authoring::DefinitionReader{library_root}.read_settings_file(path)};
    if (!source) {
        return std::unexpected{std::move(source.error())};
    }
    return sim::resolve_simulation_settings(*source);
}
}
