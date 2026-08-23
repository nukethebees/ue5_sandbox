#pragma once

#include <Sandbox/batch_game/ProxyEntityMap.h>
#include <Sandbox/batch_game/SimulationActorClasses.h>
#include <Sandbox/batch_game/SimulationConfig.h>
#include <Sandbox/batch_game/SpatialQueryManager.h>
#include <Sandbox/batch_game/test_entity_registry/TestEntityRegistry.h>
#include <Sandbox/batch_game/TestCapitalShipFightersPhaseInterface.h>
#include <Sandbox/batch_game/TestCapitalShipsPhaseInterface.h>
#include <Sandbox/batch_game/TestLasersPhaseInterface.h>
#include <Sandbox/batch_game/TestMissionManager.h>
#include <Sandbox/batch_game/TestSpaceShipPhaseInterface.h>
#include <Sandbox/batch_game/TestStaticTurretsPhaseInterface.h>
#include <Sandbox/batch_game/TestTubeSpinnersPhaseInterface.h>
#include <Sandbox/ui/HUDManager.h>
#include <Sandbox/utilities/FixedTickLoop.h>

#include <CoreMinimal.h>
#include <GameFramework/Actor.h>

#include "TestBatchOrchestrator.generated.h"

class ATestSpaceShip;
class ATestLasers;
class ATestCapitalShips;
class ATestCapitalShipFighters;
class ATestStaticTurrets;
class ATestTubeSpinners;

class ADelayedNiagaraSpawner;
class UTestSimulationConfig;

class ATestBatchOrchestrator;

DECLARE_DELEGATE_OneParam(FOrchestratorEndTickTestHook, ATestBatchOrchestrator&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnProxyEntitiesBound, FProxyEntityMap const&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnOrchestratorReset, ATestBatchOrchestrator&);

UENUM(BlueprintType)
enum class EOrchestratorStartMode : uint8 {
    Paused,
    PausedInTest,
    Automatic,
};

UENUM(BlueprintType)
enum class EOrchestratorState : uint8 {
    Uninitialised,
    Paused,
    Running,
    Stopped,
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
    void pause_simulation();
    void reset_for_new_level();

    void set_test_config(UTestSimulationConfig const& config);
    void set_start_mode(EOrchestratorStartMode mode);
    void set_assets(USimulationConfig* const assets,
                    ESimulationAssetActorScope const actor_scope,
                    ESimulationAssetProxyMode const proxy_mode);
    void set_time_scale(time_type scale) noexcept;

    auto frequency_to_tick_period(time_type const frequency) const noexcept -> tick_type;
    auto duration_to_tick_period(time_type const duration) const noexcept -> tick_type;

    auto get_state() const noexcept -> EOrchestratorState { return state; }
    auto get_completed_ticks() const noexcept -> tick_type { return completed_ticks; }
    auto get_simulation_time() const noexcept -> time_type {
        return static_cast<time_type>(completed_ticks) * simulation_tick_loop.tick_period;
    }
    auto get_tick_period() const noexcept -> time_type { return simulation_tick_loop.tick_period; }

    auto get_player_ship() const -> ATestSpaceShip const*;
    void set_player_ship(ATestSpaceShip& new_player_ship);
    auto get_lasers() const -> auto const* { return lasers.Get(); }
    auto get_capital_ships() const -> auto const* { return capital_ships.Get(); }
    auto get_capital_ship_fighters() const -> auto const* { return capital_ship_fighters.Get(); }
    auto get_turrets() const -> auto const* { return turrets.Get(); }
    auto get_spinners() const -> auto const* { return spinners.Get(); }

    auto get_entity_registry() noexcept -> FTestEntityRegistry& { return entity_registry; }
    auto get_entity_registry() const noexcept -> FTestEntityRegistry const& {
        return entity_registry;
    }
    auto get_entity_type(FRegistryEntityHandle const handle) const -> ETestEntityType {
        return entity_registry.get_entity_type(handle);
    }
    auto get_spatial_query_manager() noexcept -> ml::FSpatialQueryManager& { return query_manager; }
    auto get_spatial_query_manager() const noexcept -> ml::FSpatialQueryManager const& {
        return query_manager;
    }
    auto get_mission_manager() noexcept -> FTestMissionManager& { return mission_manager; }
    auto get_mission_manager() const noexcept -> FTestMissionManager const& {
        return mission_manager;
    }
    auto get_niagara_spawner() const -> ADelayedNiagaraSpawner const* { return niagara_spawner; }
    auto get_hud_manager() noexcept -> FHUDManager& { return hud_manager; }
    auto get_hud_manager() const noexcept -> FHUDManager const& { return hud_manager; }
    auto get_hud_update_frequencies() const noexcept -> FTestBatchGameUiUpdateFrequencies const& {
        return hud_update_frequencies;
    }
    auto get_hud_tick_loop() const noexcept { return hud_tick_loop; }

    void set_end_tick_test_hook(FOrchestratorEndTickTestHook hook);
    void clear_end_tick_test_hook();

    void spawn_missing_actors();

    static FOnProxyEntitiesBound on_proxy_entities_bound;
    FOnOrchestratorReset on_reset;
  protected:
    void BeginPlay() override;
    void EndPlay(EEndPlayReason::Type end_play_reason) override;

#if WITH_EDITOR
    UFUNCTION(CallInEditor, Category = "Sandbox|Assets")
    void apply_simulation_config();

    UFUNCTION(CallInEditor, Category = "Sandbox")
    void spawn_missing_actors_button();
#endif
  private:
    void begin_play();
    auto should_initialise_in_begin_play() const noexcept -> bool;
    void validate_proxy_handles();
    void bind_simulation_dependencies();
    void start_visual_logging();
    void stop_visual_logging();

    FOrchestratorEndTickTestHook end_tick_test_hook;

    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (ShowOnlyInnerProperties))
    FFixedTickLoop simulation_tick_loop{};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    EOrchestratorStartMode start_mode{EOrchestratorStartMode::Automatic};
    UPROPERTY(VisibleAnywhere, Transient, Category = "Sandbox")
    EOrchestratorState state{EOrchestratorState::Uninitialised};

    UPROPERTY(EditAnywhere, Category = "Sandbox|Visual Logger")
    bool enable_visual_logging{false};

    UPROPERTY(EditAnywhere, Category = "Sandbox|Assets")
    TObjectPtr<USimulationConfig> simulation_config{nullptr};

    FFixedTickLoop hud_tick_loop{};

    tick_type completed_ticks{0};

    FHUDManager hud_manager;
    FTestEntityRegistry entity_registry;
    ml::FSpatialQueryManager query_manager;

    UPROPERTY(EditAnywhere, Category = "Sandbox|UI", meta = (ShowOnlyInnerProperties))
    FTestBatchGameUiUpdateFrequencies hud_update_frequencies{};

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

    ml::test_space_ship::PhaseInterface player_ship_phase;
    ml::test_lasers::PhaseInterface lasers_phase;
    ml::test_capital_ships::PhaseInterface capital_ships_phase;
    ml::test_capital_ship_fighters::PhaseInterface capital_ship_fighters_phase;
    ml::test_static_turrets::PhaseInterface turrets_phase;
    ml::test_tube_spinners::PhaseInterface spinners_phase;

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    FTestMissionManager mission_manager{};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ADelayedNiagaraSpawner> niagara_spawner{nullptr};

    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (ShowOnlyInnerProperties))
    FSimulationActorClasses actor_classes;

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "Sandbox|Assets")
    ESimulationAssetActorScope simulation_asset_actor_scope{
        ESimulationAssetActorScope::OrchestratorActors};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Assets")
    ESimulationAssetProxyMode simulation_asset_proxy_mode{ESimulationAssetProxyMode::Include};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    bool log_ticks{false};
#endif
};
