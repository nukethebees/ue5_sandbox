#pragma once

#include <Sandbox/batch_game/ProxyEntityMap.h>
#include <Sandbox/batch_game/SimulationClockInterface.h>
#include <Sandbox/batch_game/test_entity_registry/RegistryEntityHandle.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityUniqueId.h>
#include <Sandbox/batch_game/TestEntityType.h>
#include <Sandbox/batch_game/TestMissionFailReason.h>
#include <Sandbox/batch_game/TestMissionMode.h>
#include <Sandbox/batch_game/TestMissionState.h>
#include <Sandbox/health/ShipHealth.h>

#include <CoreMinimal.h>
#include <GameFramework/Actor.h>

#include "TestMissionManager.generated.h"

class ATestEntityRegistry;
class ATestBatchOrchestrator;

DECLARE_MULTICAST_DELEGATE_OneParam(FTestMissionEndedDelegate, ATestMissionManager const&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMissionManagerReady, ATestMissionManager const&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMissionManagerUpdate, ATestMissionManager const&);

USTRUCT()
struct FTestMissionStartupData {
    GENERATED_BODY()

    void prune_invalid_actors();

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TArray<TObjectPtr<AActor>> hero_entities;

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TArray<TObjectPtr<AActor>> entities_must_survive;
};

UCLASS()
class SANDBOX_API ATestMissionManager : public AActor {
    GENERATED_BODY()
  public:
    void begin_play();
    void bind_simulation_clock(ATestBatchOrchestrator const& orchestrator) noexcept;
    void mission_tick();

    void set_mission_mode(ETestMissionMode new_mode);
    void set_target_time(float new_target_time);
    void set_kill_target(int32 new_kill_target);
    void set_save_mission_results(bool should_save) noexcept;
    void add_hero_entity(AActor& actor);
    void add_entity_that_must_survive(AActor& actor);

    void on_proxy_entities_bound(FProxyEntityMap const& proxy_entities);

    // Accessors
    auto get_mission_mode() const noexcept -> ETestMissionMode { return mission_mode; }
    auto get_mission_state() const noexcept -> ETestMissionState { return mission_state; }

    auto get_survive_seconds() const noexcept -> float { return target_time; }
    auto get_target_time() const noexcept -> float { return target_time; }
    auto get_time_remaining() const noexcept -> float {
        return FMath::Max(0.f, target_time - mission_elapsed_seconds);
    }
    auto get_kill_target() const noexcept -> int32 { return kill_target; }
    auto get_kills_remaining() const noexcept -> int32 {
        return FMath::Max(0, kill_target - mission_kills);
    }
    auto get_mission_kills() const noexcept -> int32 { return mission_kills; }
    auto get_mission_fail_reason() const noexcept -> ETestMissionFailReason {
        return mission_fail_reason;
    }
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

    auto get_mission_stopwatch() const noexcept -> float { return mission_elapsed_seconds; }
    auto mission_running() const noexcept -> bool {
        return mission_state == ETestMissionState::Running;
    }
    auto is_ready() const noexcept -> bool;

    auto get_entity_registry() const -> ATestEntityRegistry const* { return entity_registry; }
    auto get_entity_registry() -> ATestEntityRegistry* { return entity_registry; }
    void set_entity_registry(ATestEntityRegistry& reg) { entity_registry = &reg; }

    FTestMissionEndedDelegate on_mission_ended;
    FOnMissionManagerReady on_ready;
    FOnMissionManagerUpdate on_mission_update;
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

    void handle_mission_ended(ETestMissionFailReason const fail_reason);
    void handle_mission_success();
    void handle_mission_failure(ETestMissionFailReason fail_reason);

    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    TObjectPtr<ATestEntityRegistry> entity_registry{nullptr};

    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (ShowOnlyInnerProperties))
    FTestMissionStartupData startup_data{};

    TArray<FRegistryEntityHandle> hero_entity_handles{};
    TArray<TestEntityUniqueId> hero_entity_ids{};
    TArray<FRegistryEntityHandle> entity_handles_that_must_survive{};
    TArray<TestEntityUniqueId> entity_ids_that_must_survive{};
    TArray<ETestEntityType> entity_types_that_must_survive{};
    TArray<FShipHealth> entity_health_that_must_survive{};

    UPROPERTY(VisibleAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    ETestMissionState mission_state{ETestMissionState::NotStarted};

    UPROPERTY(VisibleAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    ETestMissionFailReason mission_fail_reason{ETestMissionFailReason::None};

    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (AllowPrivateAccess))
    ETestMissionMode mission_mode{ETestMissionMode::None};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    float target_time{60.0f};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    int32 kill_target{5};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    bool save_mission_results{true};

    UPROPERTY(VisibleAnywhere, Category = "Sandbox")
    int32 mission_kills{0};

    UPROPERTY(VisibleAnywhere, Category = "Sandbox")
    float mission_elapsed_seconds{0.0f};

    ml::test_batch_orchestrator::SimulationClockInterface simulation_clock;
};
