#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

#include <SpaceGame/entities/ProxyEntityMap.h>
#include <SpaceGame/entities/TestTeam.h>

#include <SandboxCore/time_series_data.h>
#include <SandboxNative/RegistryEntityHandle.h>

namespace ml {
enum class ETurretCombatScenario : uint8 { KillEnemy, ZeroDamage };

class FTurretCombatScenario final : public FSimulationTestScenario {
    static constexpr time_type test_time{3.0};
    static constexpr ETestTeam hero_team{ETestTeam::Blue};
    static constexpr ETestTeam enemy_team{ETestTeam::Red};
    inline static FTimespan const timeout{0, 0, 4};
  public:
    FTurretCombatScenario(FSimulationTestContext& context, ETurretCombatScenario scenario);
    void run() override;
  private:
    void on_tear_down() override;
    void spawn_fixture();
    void bind_proxy_entities(FProxyEntityMap const& proxy_entities);
    void sample_values();
    void on_end_tick(ATestBatchOrchestrator& orchestrator);
    void initial_setup();
    void check_initial_state();
    void check_kill_enemy_results();
    void check_zero_damage_results();

    ETurretCombatScenario scenario_;
    TimeSeriesData<int32> unique_ids;
    TimeSeriesData<int32> kills;
    TimeSeriesData<int32> alive;
    TimeSeriesData<TArray<FRegistryEntityHandle>> target_handles;
    TimeSeriesData<TArray<int32>> turret_healths;
    TArray<FRegistryEntityHandle> turret_handles;
    TArray<ETestTeam> turret_teams;
    FRegistryEntityHandle enemy_handle;
    bool proxy_entities_bound{false};
    int32 hero_health{100000};
};
}
