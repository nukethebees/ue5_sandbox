#pragma once

#include <CoreMinimal.h>
#include <GameFramework/Actor.h>

#include "TestMissionManager.generated.h"

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

UCLASS()
class ATestMissionManager : public AActor {
    GENERATED_BODY()
  public:
    void begin_play();
    void mission_tick(float const dt);

    auto get_mission_mode() const noexcept -> ETestMissionMode { return mission_mode; }
    auto get_mission_state() const noexcept -> ETestMissionState { return mission_state; }

    auto get_survive_seconds() const noexcept -> float { return survive_seconds; }
    auto get_kill_target() const noexcept -> int32 { return kill_target; }

    auto get_mission_stopwatch() const noexcept -> float { return mission_elapsed_seconds; }
    auto is_ready() const noexcept -> bool;

    FTestMissionEndedDelegate on_mission_ended;
    FOnMissionManagerReady on_ready;
  private:
    void set_mission_mode(ETestMissionMode const new_mode);
    void set_mission_state(ETestMissionState const new_state);

    void mission_tick_survive_seconds(float const dt);
    void mission_tick_kill_enemies(float const dt);

    void handle_mission_success();
    void handle_mission_failure();

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
