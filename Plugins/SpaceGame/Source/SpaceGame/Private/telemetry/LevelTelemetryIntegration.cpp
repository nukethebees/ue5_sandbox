#include <SpaceGame/telemetry/LevelTelemetryIntegration.h>

#include <ioj/sim/levels/level_sim.h>
#include <SpaceGame/system/GameSubsystem.h>
#include <SpaceGame/telemetry/LevelTelemetryJson.h>
#include <SpaceGameSimulation/support/logging/SandboxLogCategories.h>

#include <CoreGlobals.h>

#include <utility>

namespace ioj {
auto take_finalized_level_telemetry_report(sim::LevelSim& simulation,
                                           FLevelLaunchOptions const& launch_options,
                                           bool const presentation_enabled)
    -> TOptional<sim::LevelTelemetryReport> {
    auto record{simulation.take_finalized_telemetry_run()};
    if (!record.has_value()) {
        return {};
    }

    sim::LevelTelemetryReport report{std::move(record.value())};
    report.started_paused = launch_options.launch_mode == ELevelLaunchMode::Paused;
    report.opens_telemetry =
        launch_options.results_navigation == ELevelResultsNavigation::Telemetry;
    report.presentation_enabled = presentation_enabled;
    return report;
}

void persist_finalized_level_telemetry_run(sim::LevelSim& simulation,
                                           FLevelLaunchOptions const& launch_options,
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
    FString error;
    if (path) {
        UE_LOG(LogSandbox, Display, TEXT("Wrote level telemetry run to '%s'"), **path);
    } else {
        UE_LOG(LogSandbox, Error, TEXT("Failed to write level telemetry run: %s"), *path.error());
        error = path.error();
    }

    if (launch_options.results_navigation != ELevelResultsNavigation::Telemetry) {
        return;
    }

    auto* const subsystem{UGameSubsystem::get(game_instance)};
    if (!IsValid(subsystem)) {
        UE_LOG(LogSandbox, Error, TEXT("Cannot navigate to telemetry: subsystem is unavailable"));
        return;
    }
    if (!subsystem->return_to_telemetry(UTF8_TO_TCHAR(run_id.c_str()), MoveTemp(error))) {
        UE_LOG(LogSandbox, Error, TEXT("Cannot navigate to completed telemetry run"));
    }
}
}
