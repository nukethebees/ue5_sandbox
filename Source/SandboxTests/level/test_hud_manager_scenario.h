#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

#include <SpaceGame/entities/TestEntityType.h>
#include <SpaceGame/entities/TestTeam.h>
#include <SpaceGame/missions/TestMissionState.h>
#include <SpaceGame/presentation/HUDManager.h>

#include <SandboxCore/time_series_data.h>

namespace ml {
enum class EHUDManagerScenario : uint8 {
    InitialCachesPopulateWithoutHUD,
    EntityCountPollingContinuesWithoutHUD,
    MissionAndDefenceDataUpdateWithoutHUD,
    PlayerStateAndKillsUpdateWithoutHUD,
    MissionTimeUsesSimulationClockWithoutHUD,
    LateHUDRegistrationSynchronisesAndUnregisters,
};

void run_worldless_hud_manager_scenario(FAutomationTestBase& test,
                                        FSoftTestAssertions& checks,
                                        USpaceGameLevelConfig const& config,
                                        EHUDManagerScenario scenario);

class FTestHUDManagerScenario final : public FSimulationTestScenario {
    inline static FTimespan const timeout{0, 0, 2};
    static constexpr double early_sample_time{0.1};
    static constexpr double damage_queue_time{0.1};
    static constexpr double test_duration{0.35};

    struct FEntityCountSample {
        int32 cached_alive_count{0};
        int32 registered_hud_count{0};
    };
    struct FDefenceSample {
        ETestMissionState mission_state{ETestMissionState::NotStarted};
        int32 defended_entity_health{INDEX_NONE};
        int32 required_kill_entity_health{INDEX_NONE};
        float mission_stopwatch{0.f};
        int32 registered_hud_count{0};
    };
    struct FPlayerSample {
        int32 points{0};
        int32 top_killer_kills{0};
        int32 player_team_matrix_kills{0};
        int32 registered_hud_count{0};
    };
    struct FMissionTimeSample {
        float cached_time{0.f};
        float mission_time{0.f};
        int32 registered_hud_count{0};
    };
  public:
    FTestHUDManagerScenario(FSimulationTestContext& context, EHUDManagerScenario scenario);
    void run() override;
  private:
    void on_tear_down() override;
    void initial_caches_process_samples();
    void defence_pre_begin_play(UWorld& world, USpaceGameLevelConfig const& config);
    void configure_defence_mission(UWorld& world, ATestBatchOrchestrator& orchestrator);
    void defence_begin();
    void defence_on_tick(ATestBatchOrchestrator& orchestrator);
    void defence_process_samples();
    void mission_time_pre_begin_play(UWorld& world, USpaceGameLevelConfig const& config);
    void mission_time_begin();
    void mission_time_on_tick(ATestBatchOrchestrator& orchestrator);
    void mission_time_process_samples();
    void player_kill_pre_begin_play(UWorld& world, USpaceGameLevelConfig const& config);
    void player_kill_begin();
    void player_kill_on_tick(ATestBatchOrchestrator& orchestrator);
    void player_kill_process_samples();
    void registration_process_samples();
    void entity_count_pre_begin_play(UWorld& world, USpaceGameLevelConfig const& config);
    void entity_count_begin();
    void entity_count_on_tick(ATestBatchOrchestrator& orchestrator);
    void entity_count_process_samples();
    auto initialise_headless_hud_manager() -> bool;
    auto get_headless_hud_manager() -> FHUDManager&;
    void tick_headless_hud_manager();
    void check_headless_hud_manager_matches_simulation();
    void check_mission_data_equal(hud_manager::FMissionDataCache const& local,
                                  hud_manager::FMissionDataCache const& simulation);
#if WITH_EDITOR
    void check_sampled_speed_data_equal(hud_manager::FSampledSpeedDataCache const& local,
                                        hud_manager::FSampledSpeedDataCache const& simulation);
#endif
    static auto count_cached_entities(FHUDManager const& manager) -> int32;

    TOptional<FHUDManager> headless_hud_manager{NullOpt};
    int32 initial_alive_count{0};
    TimeSeriesData<FEntityCountSample> entity_count_samples;
    TimeSeriesData<FDefenceSample> defence_samples;
    TimeSeriesData<FPlayerSample> player_samples;
    TimeSeriesData<FMissionTimeSample> mission_time_samples;
    EHUDManagerScenario scenario_;
};
}
