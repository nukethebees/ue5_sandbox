#include <SpaceGame/telemetry/LevelTelemetryIntegration.h>

#include <ioj/levels/launch_options.h>
#include <ioj/sim/levels/level_sim.h>
#include <ioj/sim/telemetry/run_report.h>
#include <SpaceGame/telemetry/LevelTelemetryJson.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCoreEngine/strings.h>

#include <utility>

namespace ioj {
auto make_level_telemetry_report(sim::LevelSim const& simulation,
                                 ::ioj::levels::LaunchOptions const& launch_options,
                                 bool const presentation_enabled)
    -> TOptional<sim::telemetry::Report> {
    auto record{simulation.telemetry_run()};
    if (!record.has_value()) {
        return {};
    }

    return sim::telemetry::make_run_report(
        std::move(*record), launch_options, presentation_enabled);
}

auto persist_level_telemetry_run(sim::LevelSim const& simulation,
                                 ::ioj::levels::LaunchOptions const& launch_options,
                                 bool const presentation_enabled)
    -> std::expected<std::string, std::string> {
    auto const report{
        make_level_telemetry_report(simulation, launch_options, presentation_enabled)};
    auto const path{write_level_telemetry_run(report.GetValue(), level_telemetry_runs_directory())};
    if (path) {
        UE_LOG(LogSandbox, Display, TEXT("Wrote level telemetry run to '%s'"), **path);
    } else {
        UE_LOG(LogSandbox, Error, TEXT("Failed to write level telemetry run: %s"), *path.error());
    }

    return path.transform([&report](FString const&) { return report->run.metadata.run_id; })
        .transform_error([](FString const& error) { return to_string(error); });
}
}
