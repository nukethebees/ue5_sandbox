#pragma once

#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityUniqueId.h>

#include <CoreMinimal.h>
#include <GameFramework/Actor.h>

#include "TestMissionManager.generated.h"

class ATestEntityRegistry;
class ATestSpaceShip;

UENUM()
enum class ETestMissionState : uint8 {
    NotStarted,
    Running,
    Succeeded,
    Failed,
    Disabled,
};

UENUM()
enum class ETestMissionMode : uint8 {
    None,
    SurviveTime,
    KillEnemies,
};

DECLARE_MULTICAST_DELEGATE_OneParam(FTestMissionEndedDelegate, ATestMissionManager const&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMissionManagerReady, ATestMissionManager const&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMissionManagerUpdate, ATestMissionManager const&);

UCLASS()
class ATestMissionManager : public AActor {
    GENERATED_BODY()
  public:
    void begin_play();
    void mission_tick(float const dt);

    void update_player_handles();

    auto get_mission_mode() const noexcept -> ETestMissionMode { return mission_mode; }
    auto get_mission_state() const noexcept -> ETestMissionState { return mission_state; }

    auto get_survive_seconds() const noexcept -> float { return survive_seconds; }
    auto get_kill_target() const noexcept -> int32 { return kill_target; }
    auto get_player_kills() const noexcept -> int32 { return player_kills; }

    auto get_mission_stopwatch() const noexcept -> float { return mission_elapsed_seconds; }
    auto mission_running() const noexcept -> bool {
        return mission_state == ETestMissionState::Running;
    }
    auto is_ready() const noexcept -> bool;

    auto get_player_id() const { return player_id; }

    FTestMissionEndedDelegate on_mission_ended;
    FOnMissionManagerReady on_ready;
    FOnMissionManagerUpdate on_mission_update;
  private:
    void set_mission_mode(ETestMissionMode const new_mode);
    void set_mission_state(ETestMissionState const new_state);

    void mission_tick_survive_seconds(float const dt);
    void mission_tick_kill_enemies(float const dt);

    void handle_mission_success();
    void handle_mission_failure();

    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    TObjectPtr<ATestEntityRegistry> entity_registry{nullptr};

    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    TObjectPtr<ATestSpaceShip> player_ship{nullptr};

    FRegistryEntityHandle player_registry_handle{};
    TestEntityUniqueId player_id{};

    UPROPERTY(VisibleAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    ETestMissionState mission_state{ETestMissionState::NotStarted};

    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    ETestMissionMode mission_mode{ETestMissionMode::None};

    UPROPERTY(EditAnywhere)
    float survive_seconds{60.0f};

    UPROPERTY(EditAnywhere)
    int32 kill_target{5};

    UPROPERTY(EditAnywhere)
    int32 player_kills{0};

    UPROPERTY(VisibleAnywhere)
    float mission_elapsed_seconds{0.0f};

    UPROPERTY(VisibleAnywhere)
    float mission_start_time{0.0f};
};
