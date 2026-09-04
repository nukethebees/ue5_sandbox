#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

#include <SpaceGame/ships/capital/TestCapitalShipsSimulation.h>
#include <SpaceGame/ships/fighters/TestCapitalShipFightersSimulation.h>
#include <SpaceGame/simulation/TestBatchOrchestrator.h>

#include <SandboxCore/time_series_data.h>
#include <SandboxNative/RegistryEntityHandle.h>

class ATestSpaceShip;

namespace ml {
class FPlayerShipVsCapitalScenario final : public FSimulationTestScenario {
    static constexpr time_type initial_wait{0.1};
    static constexpr time_type track_time{0.5};
    static constexpr time_type t_start{0.0};
    static constexpr time_type t_settled{t_start + initial_wait};
    static constexpr time_type t_tracked{t_settled + track_time};
    static constexpr time_type t_end{t_tracked + 5.0};
    static constexpr time_type sample_time_before_end{0.5};
  public:
    explicit FPlayerShipVsCapitalScenario(FSimulationTestContext& context);
    void run() override;
  private:
    void spawn_fixture();
    void sample_values(ATestBatchOrchestrator& orchestrator);
    void on_end_tick(ATestBatchOrchestrator& orchestrator);
    void initial_setup();
    void full_checks();
    void fail_self_analysis();
    void export_failure_data() const;

    ATestSpaceShip const* player_ship{nullptr};
    test_capital_ships::Simulation const* capitals{nullptr};
    test_capital_ship_fighters::Simulation const* fighters{nullptr};
    FRegistryEntityHandle player_ship_handle;
    TimeSeriesData<FVector> player_ship_locations;
    TimeSeriesData<FVector> player_ship_registry_locations;
    TimeSeriesData<TArray<FVector3f>> fighter_target_locations;
    TimeSeriesData<TArray<FVector3f>> fighter_locations;
    TimeSeriesData<ATestBatchOrchestrator::tick_type> orchestrator_ticks;
    int32 i_setup{INDEX_NONE};
    int32 i_tracked{INDEX_NONE};
    int32 i_before_end{INDEX_NONE};
    int32 i_end{INDEX_NONE};
    FTimespan timeout{0, 0, FMath::CeilToInt32(t_end + 1.0)};
};
}
