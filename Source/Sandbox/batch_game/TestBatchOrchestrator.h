#pragma once

#include <Sandbox/batch_game/SimulationActorClasses.h>
#include <Sandbox/batch_game/SimulationConfig.h>

#include <CoreMinimal.h>
#include <GameFramework/Actor.h>

#include "TestBatchOrchestrator.generated.h"

class ATestSpaceShip;
class ATestLasers;
class ATestCapitalShips;
class ATestCapitalShipFighters;
class ATestStaticTurrets;
class ATestTubeSpinners;

class ATestEntityRegistry;
class ATestMissionManager;
class ADelayedNiagaraSpawner;

class ATestBatchOrchestrator;

DECLARE_DELEGATE_OneParam(FOrchestratorEndTickTestHook, ATestBatchOrchestrator&);

UENUM(BlueprintType)
enum class EOrchestratorStartMode : uint8 {
    Paused,
    PausedInTest,
    Automatic,
};

UCLASS()
class SANDBOX_API ATestBatchOrchestrator : public AActor {
    GENERATED_BODY()
  public:
    using tick_type = uint64;
    using time_type = double;

    ATestBatchOrchestrator();

    void Tick(float dt) override;
    void tick(time_type const dt);
    void start_simulation();

    void set_assets(USimulationConfig* const assets,
                    ESimulationAssetActorScope const actor_scope,
                    ESimulationAssetProxyMode const proxy_mode);
    void set_time_scale(time_type scale) noexcept;

    auto frequency_to_tick_period(time_type const frequency) const noexcept -> tick_type;
    auto duration_to_tick_period(time_type const duration) const noexcept -> tick_type;

    auto get_completed_ticks() const noexcept -> tick_type { return completed_ticks; }
    auto get_simulation_time() const noexcept -> time_type {
        return static_cast<time_type>(completed_ticks) * tick_period;
    }

    auto get_player_ship() const -> auto const* { return player_ship.Get(); }
    auto get_lasers() const -> auto const* { return lasers.Get(); }
    auto get_capital_ships() const -> auto const* { return capital_ships.Get(); }
    auto get_capital_ship_fighters() const -> auto const* { return capital_ship_fighters.Get(); }
    auto get_turrets() const -> auto const* { return turrets.Get(); }
    auto get_spinners() const -> auto const* { return spinners.Get(); }

    auto get_entity_registry() const { return entity_registry; }

    void set_end_tick_test_hook(FOrchestratorEndTickTestHook hook);
    void clear_end_tick_test_hook();
  protected:
    void BeginPlay() override;

#if WITH_EDITOR
    UFUNCTION(CallInEditor, Category = "Sandbox|Assets")
    void apply_simulation_config();

    UFUNCTION(CallInEditor, Category = "Sandbox")
    void spawn_missing_actors();
#endif
  private:
    void begin_play();
    void validate_proxy_handles();
    void bind_simulation_dependencies();

    FOrchestratorEndTickTestHook end_tick_test_hook;

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    double tick_rate{60.f};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    double time_scale{1.f};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    EOrchestratorStartMode start_mode{EOrchestratorStartMode::Automatic};

    UPROPERTY(EditAnywhere, Category = "Sandbox|Assets")
    TObjectPtr<USimulationConfig> simulation_config{nullptr};

    time_type tick_period{0.f};
    time_type accumulator{0.f};

    tick_type completed_ticks{0};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestSpaceShip> player_ship{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestLasers> lasers{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestCapitalShips> capital_ships{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestCapitalShipFighters> capital_ship_fighters{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestStaticTurrets> turrets{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestTubeSpinners> spinners{nullptr};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestEntityRegistry> entity_registry{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestMissionManager> mission_manager{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ADelayedNiagaraSpawner> niagara_spawner{nullptr};

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (ShowOnlyInnerProperties))
    FSimulationActorClasses actor_classes;

    UPROPERTY(EditAnywhere, Category = "Sandbox|Assets")
    ESimulationAssetActorScope simulation_asset_actor_scope{
        ESimulationAssetActorScope::OrchestratorActors};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Assets")
    ESimulationAssetProxyMode simulation_asset_proxy_mode{ESimulationAssetProxyMode::Include};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    bool log_ticks{false};
#endif
};

namespace ml::test_batch_orchestrator {
class SANDBOX_API SimulationClockInterface {
  public:
    inline void bind(ATestBatchOrchestrator const& new_orchestrator) noexcept {
        orchestrator = &new_orchestrator;
    }

    inline auto frequency_to_tick_period(ATestBatchOrchestrator::time_type const frequency) const
        noexcept -> ATestBatchOrchestrator::tick_type {
        check(IsValid(orchestrator));
        return orchestrator->frequency_to_tick_period(frequency);
    }

    inline auto duration_to_tick_period(ATestBatchOrchestrator::time_type const duration) const
        noexcept -> ATestBatchOrchestrator::tick_type {
        check(IsValid(orchestrator));
        return orchestrator->duration_to_tick_period(duration);
    }

    inline auto get_completed_ticks() const noexcept -> ATestBatchOrchestrator::tick_type {
        check(IsValid(orchestrator));
        return orchestrator->get_completed_ticks();
    }
  private:
    ATestBatchOrchestrator const* orchestrator{nullptr};
};
}
