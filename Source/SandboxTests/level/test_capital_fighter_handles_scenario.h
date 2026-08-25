#pragma once

#include <SandboxTests/support/SimulationTestScenario.h>

#include <Sandbox/batch_game/TestBatchOrchestrator.h>
#include <Sandbox/batch_game/TestCapitalShipFighters.h>
#include <Sandbox/batch_game/TestTeam.h>

#include <SandboxCore/time_series_data.h>
#include <SandboxNative/RegistryEntityHandle.h>

class ATestCapitalShips;

namespace ml {
enum class ECapitalFighterHandlesScenario : uint8 { KillFightersOnly, KillCapital, All };

class FCapitalFighterHandlesScenario final : public FSimulationTestScenario {
    using Task = ATestCapitalShipFighters::Task;

    struct FCapitalSample {
        FRegistryEntityHandle handle;
        FRegistryEntityHandle target_handle;
    };
    struct FFighterSample {
        FRegistryEntityHandle handle{};
        FRegistryEntityHandle target_handle{};
        FVector3f target_location{FVector3f::ZeroVector};
    };
    struct FSimulationSnapshot {
        int32 fighter_spawn_slots{0};
        int32 fighter_count{0};
        TArray<FCapitalSample> capitals;
        TArray<FRegistryEntityHandle> capital_fighter_handles;
        TArray<int32> capital_fighter_counts;
        TArray<FRegistryEntityHandle> fighter_handles;
        TArray<FRegistryEntityHandle> fighter_target_handles;
        TArray<Task> fighter_tasks;
        TArray<FFighterSample> main_capital_fighters;
    };

    static constexpr ETestTeam main_capital_team{ETestTeam::Green};
    static constexpr int32 n_capitals_exp{3};
    static constexpr time_type initial_sample_delay{2.0};
    static constexpr time_type fighter_kill_delay{3.0};
    static constexpr time_type post_fighter_kill_sample_delay{2.0};
    static constexpr time_type capital_kill_delay{3.0};
    static constexpr time_type post_capital_kill_sample_delay{5.0};
    static constexpr time_type final_sample_delay{5.0};
    inline static FTimespan const default_timeout{0, 0, 5};
  public:
    FCapitalFighterHandlesScenario(FSimulationTestContext& context,
                                   ECapitalFighterHandlesScenario scenario);
    void run() override;
  private:
    void spawn_fixture();
    void sample_values(ATestBatchOrchestrator& orchestrator);
    void on_end_tick(ATestBatchOrchestrator& orchestrator);
    auto find_main_capital_index() const -> TOptional<int32>;
    void initial_setup();
    void kill_fighters();
    void kill_capital_opponent();
    void configure_timeline(bool should_kill_fighters, bool should_kill_capital);
    void check_capitals_not_targeting_self(FSimulationSnapshot const& sample);
    void check_main_capital_fighters_not_targeting_parent(FSimulationSnapshot const& sample,
                                                          FString const& description);
    void check_spawn_capital_handle_state(FSimulationSnapshot const& sample);
    void check_all_fighter_targets_not_null(FSimulationSnapshot const& sample);
    void check_fighter_kill_state(FSimulationSnapshot const& initial,
                                  FSimulationSnapshot const& after_kill);
    void check_main_capital_fighter_handles_unchanged(FSimulationSnapshot const& before,
                                                      FSimulationSnapshot const& after);
    void check_fighter_targets_changed(FSimulationSnapshot const& before,
                                       FSimulationSnapshot const& after);
    void check_capital_kill_state(FSimulationSnapshot const& before_kill,
                                  FSimulationSnapshot const& after_kill);
    auto snapshot_at(time_type time) const -> FSimulationSnapshot;
    void check_fighter_handle_counts_for_all_ticks();
    void full_checks(bool should_kill_fighters, bool should_kill_capital);
    void export_data(FName test_name) const;
    void run_test(FName test_name, bool should_kill_fighters, bool should_kill_capital);

    ECapitalFighterHandlesScenario scenario_;
    ATestCapitalShips const* capitals{nullptr};
    ATestCapitalShipFighters const* fighters{nullptr};
    TimeSeriesData<ATestBatchOrchestrator::tick_type> orchestrator_tick_samples;
    TimeSeriesData<int32> fighter_spawn_slots_samples;
    TimeSeriesData<int32> fighter_count_samples;
    TimeSeriesData<int32> capital_count_samples;
    TimeSeriesData<int32> capital_fighter_handle_count_samples;
    TimeSeriesData<int32> capital_fighter_span_count_samples;
    TimeSeriesData<int32> fighter_handle_count_samples;
    TimeSeriesData<int32> fighter_target_handle_count_samples;
    TimeSeriesData<int32> fighter_task_count_samples;
    TimeSeriesData<int32> main_capital_fighter_count_samples;
    TimeSeriesData<TArray<FCapitalSample>> capital_samples;
    TimeSeriesData<TArray<FRegistryEntityHandle>> capital_fighter_handle_samples;
    TimeSeriesData<TArray<int32>> capital_fighter_count_samples;
    TimeSeriesData<TArray<FRegistryEntityHandle>> fighter_handle_samples;
    TimeSeriesData<TArray<FRegistryEntityHandle>> fighter_target_handle_samples;
    TimeSeriesData<TArray<Task>> fighter_task_samples;
    TimeSeriesData<TArray<FFighterSample>> main_capital_fighter_samples;
    FRegistryEntityHandle main_capital_handle;
    TArray<FRegistryEntityHandle> destroyed;
    TArray<FRegistryEntityHandle> kept;
    TOptional<time_type> t_initial;
    TOptional<time_type> t_post_fighter_kill;
    TOptional<time_type> t_pre_capital_kill;
    TOptional<time_type> t_post_capital_kill;
};
}
