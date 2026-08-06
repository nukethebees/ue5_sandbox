#pragma once

#include <Sandbox/batch_game/SimulationClockInterface.h>
#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityUniqueId.h>
#include <Sandbox/batch_game/TestMissionFailReason.h>
#include <Sandbox/batch_game/TestMissionMode.h>
#include <Sandbox/batch_game/TestMissionState.h>

#include <CoreMinimal.h>
#include <GameFramework/Actor.h>

#include "TestMissionManager.generated.h"

class ATestEntityRegistry;
class ATestBatchOrchestrator;
class ATestSpaceShip;

DECLARE_MULTICAST_DELEGATE_OneParam(FTestMissionEndedDelegate, ATestMissionManager const&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMissionManagerReady, ATestMissionManager const&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMissionManagerUpdate, ATestMissionManager const&);

USTRUCT()
struct FTestMissionStartupData {
    GENERATED_BODY()

    void prune_invalid_actors();

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TArray<TObjectPtr<AActor>> entities_must_survive;
};

UCLASS()
class ATestMissionManager : public AActor {
    GENERATED_BODY()
  public:
    void begin_play();
    void bind_simulation_clock(ATestBatchOrchestrator const& orchestrator) noexcept;
    void mission_tick();

    void update_player_handles();
    void on_proxy_handles_bound(
        TMap<AActor const*, FRegistryEntityHandle> const& proxy_handles);

    // Accessors
    auto get_mission_mode() const noexcept -> ETestMissionMode { return mission_mode; }
    auto get_mission_state() const noexcept -> ETestMissionState { return mission_state; }

    auto get_survive_seconds() const noexcept -> float { return target_time; }
    auto get_target_time() const noexcept -> float { return target_time; }
    auto get_kill_target() const noexcept -> int32 { return kill_target; }
    auto get_kills_remaining() const noexcept -> int32 { return kill_target - player_kills; }
    auto get_player_kills() const noexcept -> int32 { return player_kills; }

    auto get_mission_stopwatch() const noexcept -> float { return mission_elapsed_seconds; }
    auto mission_running() const noexcept -> bool {
        return mission_state == ETestMissionState::Running;
    }
    auto is_ready() const noexcept -> bool;

    auto get_player_id() const { return player_id; }

    auto get_entity_registry() const -> ATestEntityRegistry const* { return entity_registry; }
    void set_entity_registry(ATestEntityRegistry& reg) { entity_registry = &reg; }

    auto get_player_ship() const -> ATestSpaceShip const* { return player_ship; }
    void set_player_ship(ATestSpaceShip& new_ref) { player_ship = &new_ref; }

    FTestMissionEndedDelegate on_mission_ended;
    FOnMissionManagerReady on_ready;
    FOnMissionManagerUpdate on_mission_update;
  private:
    void set_mission_mode(ETestMissionMode const new_mode);
    void set_mission_state(
        ETestMissionState const new_state,
        ETestMissionFailReason const fail_reason = ETestMissionFailReason::None);

    void mission_tick_survive_seconds();
    void mission_tick_kill_enemies();
    void mission_tick_kill_enemies_within_time();
    auto entities_that_must_survive_are_alive() const -> bool;

    void handle_mission_ended(ETestMissionFailReason const fail_reason);
    void handle_mission_success();
    void handle_mission_failure(ETestMissionFailReason fail_reason);

    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    TObjectPtr<ATestEntityRegistry> entity_registry{nullptr};

    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    TObjectPtr<ATestSpaceShip> player_ship{nullptr};

    FRegistryEntityHandle player_registry_handle{};
    TestEntityUniqueId player_id{};

    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (ShowOnlyInnerProperties))
    FTestMissionStartupData startup_data{};

    TArray<FRegistryEntityHandle> entity_handles_that_must_survive{};

    UPROPERTY(VisibleAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    ETestMissionState mission_state{ETestMissionState::NotStarted};

    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    ETestMissionMode mission_mode{ETestMissionMode::None};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    float target_time{60.0f};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    int32 kill_target{5};

    UPROPERTY(VisibleAnywhere, Category = "Sandbox")
    int32 player_kills{0};

    UPROPERTY(VisibleAnywhere, Category = "Sandbox")
    float mission_elapsed_seconds{0.0f};

    ml::test_batch_orchestrator::SimulationClockInterface simulation_clock;
};
