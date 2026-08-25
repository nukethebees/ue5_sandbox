#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

#include <SandboxCore/time_series_data.h>
#include <SandboxNative/RegistryEntityHandle.h>

class ATestCapitalShipFighters;
class ATestCapitalShips;

namespace ml {
class FFightersInterceptCapitalScenario final : public FSimulationTestScenario {

    struct FSimulationSample {
        FRegistryEntityHandle parent_target;
        TArray<FRegistryEntityHandle> fighter_targets;
        int32 fighter_count{0};
    };

    static constexpr time_type test_duration{20.0};
    static constexpr time_type initial_setup_duration{2.0 / 60.0};
  public:
    explicit FFightersInterceptCapitalScenario(FSimulationTestContext& context);
    void run() override;
  private:
    void spawn_fixture();
    void sample_values();
    void on_end_tick(ATestBatchOrchestrator& orchestrator);
    void initial_setup();
    void check_fighters_share_target(FSimulationSample const& sample, FString const& description);
    void full_checks();
    void export_data() const;

    ATestCapitalShips const* capitals{nullptr};
    ATestCapitalShipFighters const* fighters{nullptr};
    TimeSeriesData<FSimulationSample> samples;
    int32 hero_capital_index{INDEX_NONE};
    FRegistryEntityHandle hero_capital;
    FRegistryEntityHandle original_target;
    FRegistryEntityHandle intercept_target;
};
}
