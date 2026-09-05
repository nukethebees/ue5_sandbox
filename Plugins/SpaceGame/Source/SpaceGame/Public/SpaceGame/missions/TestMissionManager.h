#pragma once

#include <SandboxNative/RegistryEntityHandle.h>
#include <SpaceGame/entities/TestEntityType.h>
#include <SpaceGame/entities/TestEntityUniqueId.h>
#include <SpaceGame/levels/LevelMissionInitialisationData.h>
#include <SpaceGame/missions/TestMissionFailReason.h>
#include <SpaceGame/missions/TestMissionMode.h>
#include <SpaceGame/missions/TestMissionState.h>
#include <SpaceGame/ships/common/ShipHealth.h>
#include <SpaceGame/simulation/SimulationClockInterface.h>

#include <CoreMinimal.h>

struct FTestEntityRegistry;

namespace ml {
struct FLevelMissionEventGroupsConstView;
}

struct SPACEGAME_API FTestMissionCompletion {
    FName level_id{NAME_None};
    FString level_display_name{};
    bool persisted{false};
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTestMissionCompleted, FTestMissionCompletion const&);

struct FLevelMissionResult {
    FName level_id{NAME_None};
    FString level_display_name;
    ETestMissionMode mode{ETestMissionMode::None};
    ETestMissionState state{ETestMissionState::NotStarted};
    ETestMissionFailReason fail_reason{ETestMissionFailReason::None};
    int32 kills{};
    float elapsed_seconds{};
    int32 target_kills{};
    float target_time{};
    bool save_results{false};
};

struct SPACEGAME_API FTestMissionManager {
  public:
    void begin_play();
    void reset_runtime_state();
    void bind_simulation_clock(FSimulationClock const& clock) noexcept;
    void mission_tick();
    auto complete_mission() -> bool;

    void initialise_level_mission(ml::FLevelMissionInitialisationData const& data,
                                  TConstArrayView<FRegistryEntityHandle> level_entity_handles);
    void bind_level_event_data(TConstArrayView<int32> values,
                               TConstArrayView<FRegistryEntityHandle> level_entity_handles);
    void consume_level_events(ml::FLevelMissionEventGroupsConstView groups);

    void set_mission_mode(ETestMissionMode new_mode);
    void set_target_time(float new_target_time);
    void set_kill_target(int32 new_kill_target);
    void set_save_mission_results(bool should_save) noexcept;
    void set_level_identity(FName level_id, FString display_name);
    void add_hero_entity(FRegistryEntityHandle handle);
    void add_entity_that_must_survive(FRegistryEntityHandle handle);
    void add_entity_required_to_kill(FRegistryEntityHandle handle);
    void increase_kill_target(int32 increase);
    void set_pending_objective_events(int32 count);
    void objective_event_dispatched();

    auto take_result() -> TOptional<FLevelMissionResult>;

    // Accessors
    auto get_mission_mode() const noexcept -> ETestMissionMode { return mission_mode; }
    auto get_mission_state() const noexcept -> ETestMissionState { return mission_state; }

    auto get_survive_seconds() const noexcept -> float { return target_time; }
    auto get_target_time() const noexcept -> float { return target_time; }
    auto get_time_remaining() const noexcept -> float {
        return FMath::Max(0.f, target_time - mission_elapsed_seconds);
    }
    auto get_kill_target() const noexcept -> int32 { return resolved_kill_target; }
    auto get_kills_remaining() const noexcept -> int32 {
        return FMath::Max(0, resolved_kill_target - mission_kills);
    }
    auto get_mission_kills() const noexcept -> int32 { return mission_kills; }
    auto get_mission_fail_reason() const noexcept -> ETestMissionFailReason {
        return mission_fail_reason;
    }
    auto get_level_id() const noexcept -> FName { return level_id; }
    auto get_level_display_name() const noexcept -> FString const& { return level_display_name; }
    auto should_save_mission_results() const noexcept -> bool { return save_mission_results; }
    auto get_hero_entity_handles() const noexcept -> TConstArrayView<FRegistryEntityHandle> {
        return hero_entity_handles;
    }
    auto get_entity_handles_that_must_survive() const noexcept
        -> TConstArrayView<FRegistryEntityHandle> {
        return entity_handles_that_must_survive;
    }
    auto get_entity_health_that_must_survive() const noexcept -> TConstArrayView<FShipHealth> {
        return entity_health_that_must_survive;
    }
    auto get_entity_ids_that_must_survive() const noexcept -> TConstArrayView<TestEntityUniqueId> {
        return entity_ids_that_must_survive;
    }
    auto get_entity_types_that_must_survive() const noexcept -> TConstArrayView<ETestEntityType> {
        return entity_types_that_must_survive;
    }
    auto get_entity_handles_required_to_kill() const noexcept
        -> TConstArrayView<FRegistryEntityHandle> {
        return entity_handles_required_to_kill;
    }
    auto get_entity_health_required_to_kill() const noexcept -> TConstArrayView<FShipHealth> {
        return entity_health_required_to_kill;
    }
    auto get_entity_ids_required_to_kill() const noexcept -> TConstArrayView<TestEntityUniqueId> {
        return entity_ids_required_to_kill;
    }
    auto get_entity_types_required_to_kill() const noexcept -> TConstArrayView<ETestEntityType> {
        return entity_types_required_to_kill;
    }

    auto get_mission_stopwatch() const noexcept -> float { return mission_elapsed_seconds; }
    auto mission_running() const noexcept -> bool {
        return mission_state == ETestMissionState::Running;
    }
    auto is_ready() const noexcept -> bool;
    auto has_pending_objective_events() const noexcept -> bool {
        return pending_objective_events_ > 0;
    }

    auto get_entity_registry() const -> FTestEntityRegistry const* { return entity_registry; }
    auto get_entity_registry() -> FTestEntityRegistry* { return entity_registry; }
    void set_entity_registry(FTestEntityRegistry& reg) { entity_registry = &reg; }
  private:
    void set_mission_state(ETestMissionState const new_state,
                           ETestMissionFailReason const fail_reason = ETestMissionFailReason::None);

    void mission_tick_survive_seconds();
    void mission_tick_kill_enemies();
    void mission_tick_kill_enemies_within_time();
    void update_mission_kills();
    void initialise_entity_health_that_must_survive();
    void update_entity_health_that_must_survive();
    auto entities_that_must_survive_are_alive() const -> bool;
    void initialise_entity_health_required_to_kill();
    void update_entity_health_required_to_kill();
    auto entities_required_to_kill_are_dead() const -> bool;

    void handle_mission_success();
    void handle_mission_failure(ETestMissionFailReason fail_reason);

    void queue_result();
    TOptional<FLevelMissionResult> pending_result_;
    FTestEntityRegistry* entity_registry{nullptr};

    TArray<FRegistryEntityHandle> hero_entity_handles{};
    TArray<TestEntityUniqueId> hero_entity_ids{};
    TArray<FRegistryEntityHandle> entity_handles_that_must_survive{};
    TArray<TestEntityUniqueId> entity_ids_that_must_survive{};
    TArray<ETestEntityType> entity_types_that_must_survive{};
    TArray<FShipHealth> entity_health_that_must_survive{};
    TArray<FRegistryEntityHandle> entity_handles_required_to_kill{};
    TArray<TestEntityUniqueId> entity_ids_required_to_kill{};
    TArray<ETestEntityType> entity_types_required_to_kill{};
    TArray<FShipHealth> entity_health_required_to_kill{};

    ETestMissionState mission_state{ETestMissionState::NotStarted};

    ETestMissionFailReason mission_fail_reason{ETestMissionFailReason::None};

    ETestMissionMode mission_mode{ETestMissionMode::None};

    float target_time{60.0f};

    int32 kill_target{5};

    int32 resolved_kill_target{5};

    bool save_mission_results{true};

    FName level_id{NAME_None};
    FString level_display_name{};

    int32 mission_kills{0};

    int32 pending_objective_events_{};
    TConstArrayView<int32> level_event_values_{};
    TConstArrayView<FRegistryEntityHandle> level_entity_handles_{};
    int32 kill_target_increase_before_level_initialisation_{};
    bool level_initialisation_applied_{};

    float mission_elapsed_seconds{0.0f};

    ml::test_batch_orchestrator::SimulationClockInterface simulation_clock;
};
