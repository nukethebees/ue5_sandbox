#include <SpaceGame/telemetry/LevelTelemetryIntegration.h>

#include <ioj/levels/launch_options.h>
#include <ioj/sim/levels/level_sim.h>
#include <ioj/sim/telemetry/run_report.h>
#include <SpaceGame/system/GameSubsystem.h>
#include <SpaceGame/telemetry/LevelTelemetryJson.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <SandboxCoreEngine/strings.h>

#include <CoreGlobals.h>

#include <utility>

namespace ioj {
auto take_finalized_level_telemetry_report(sim::LevelSim& simulation,
                                           ::ioj::levels::LaunchOptions const& launch_options,
                                           bool const presentation_enabled)
    -> TOptional<sim::telemetry::Report> {
    auto record{simulation.take_finalized_telemetry_run()};
    if (!record.has_value()) {
        return {};
    }

    return sim::telemetry::make_run_report(
        std::move(*record), launch_options, presentation_enabled);
}

void persist_finalized_level_telemetry_run(sim::LevelSim& simulation,
                                           ::ioj::levels::LaunchOptions const& launch_options,
                                           bool const presentation_enabled,
                                           UGameInstance* const game_instance) {
    if (GIsAutomationTesting) {
        return;
    }

    auto finalized_report{
        take_finalized_level_telemetry_report(simulation, launch_options, presentation_enabled)};
    if (!finalized_report.IsSet()) {
        return;
    }

    auto const& report{finalized_report.GetValue()};
    auto const run_id{report.run.metadata.run_id};
    auto const path{write_level_telemetry_run(report, level_telemetry_runs_directory())};
    if (path) {
        UE_LOG(LogSandbox, Display, TEXT("Wrote level telemetry run to '%s'"), **path);
    } else {
        UE_LOG(LogSandbox, Error, TEXT("Failed to write level telemetry run: %s"), *path.error());
    }

    if (launch_options.results_navigation != ::ioj::levels::ResultsNavigation::Telemetry) {
        return;
    }

    auto* const subsystem{UGameSubsystem::get(game_instance)};
    if (!IsValid(subsystem)) {
        UE_LOG(LogSandbox, Error, TEXT("Cannot navigate to telemetry: subsystem is unavailable"));
        return;
    }
    auto result{path.transform([&run_id](FString const&) { return run_id; })
                    .transform_error([](FString const& error) { return to_string(error); })};
    if (!subsystem->return_to_telemetry(std::move(result))) {
        UE_LOG(LogSandbox, Error, TEXT("Cannot navigate to completed telemetry run"));
    }
}
}
