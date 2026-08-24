#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>
#include <SandboxTests/support/TestSimulationDriver.h>

#include <SpaceGame/entities/ProxyEntityMap.h>
#include <SpaceGame/entities/TestTeam.h>

#include <SandboxCore/time_series_data.h>
#include <SandboxNative/RegistryEntityHandle.h>

namespace ml {
class FFighterAttackScenario final : public FSimulationTestScenario {
    using time_type = TestSimulationDriver::time_type;

    struct FSimulationSample {
        int32 enemy_health{0};
        TArray<ETestTeam> fighter_teams;
        TArray<float> radii;
    };

    static constexpr time_type initial_wait{1.0};
    static constexpr time_type fight_duration{10.0};
    inline static FTimespan const timeout{0, 0, 12};
  public:
    explicit FFighterAttackScenario(FSimulationTestContext& context);
    void run() override;
    void tear_down() override;
  private:
    void spawn_fixture();
    void bind_proxy_entities(FProxyEntityMap const& proxy_entities);
    void sample_values();
    void on_end_tick(ATestBatchOrchestrator& orchestrator);
    void initial_setup_and_stimuli();
    void check_fighters_team(FSimulationSample const& sample);
    void full_checks();

    TOptional<TestSimulationDriver> test_driver{NullOpt};
    FRegistryEntityHandle hero;
    FRegistryEntityHandle enemy;
    ETestTeam hero_team{ETestTeam::Green};
    ETestTeam enemy_team{ETestTeam::Red};
    TimeSeriesData<FSimulationSample> samples;
    time_type t_pre_fight{0.0};
    time_type t_post_fight{0.0};
};
}
