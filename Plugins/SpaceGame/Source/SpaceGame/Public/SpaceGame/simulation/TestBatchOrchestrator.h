#pragma once
#include <SpaceGame/entities/ProxyEntityMap.h>
#include <SpaceGame/missions/MissionCompletion.h>
#include <SpaceGame/telemetry/LevelTelemetryReport.h>
#include <SpaceGameSimulation/support/FixedTickLoop.h>

#include <ioj/sim/level_sim.h>
#include <SpaceGame/missions/LevelMissionDefinition.h>
#include <SpaceGame/simulation/LevelCollisionHost.h>
#include <SpaceGame/simulation/LevelSimulationBuilder.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/system/GameSubsystem.h>
#include <SpaceGamePresentation/presentation/HUDManager.h>
#include <SpaceGamePresentation/presentation/LevelPresentation.h>

#include <CoreMinimal.h>
#include <GameFramework/Actor.h>

#include "TestBatchOrchestrator.generated.h"

class ATestSpaceShip;
class UCollisionGridVisualizationComponent;
class UInstancedStaticMeshComponent;
class USandboxISMCComponent;
class USparkRendererComponent;

class ATestBatchOrchestrator;

DECLARE_DELEGATE_OneParam(FOrchestratorEndTickTestHook, ATestBatchOrchestrator&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnProxyEntitiesBound, FProxyEntityMap const&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnOrchestratorReset, ATestBatchOrchestrator&);

UENUM(BlueprintType)
enum class EOrchestratorStartMode : uint8 {
    Paused,
    PausedInTest,
    Automatic,
    AuthoredLevel,
};

UCLASS()
class SPACEGAME_API ATestBatchOrchestrator : public AActor {
    GENERATED_BODY()
    friend struct FTestBatchOrchestratorTestAccess;
  public:
    using tick_type = ::ioj::sim::SimTick;
    using time_type = double;

    /* **************************************** */
    // Lifecycle and simulation control
    /* **************************************** */
    ATestBatchOrchestrator();

    void Tick(float dt) override;
    void PostLoad() override;
    void tick(time_type const dt);
    void start_simulation();
    void pause_simulation();
    void reset_for_new_level();

    /* **************************************** */
    // Level configuration
    /* **************************************** */
    void set_level_config(USpaceGameLevelConfig& config);
    auto get_level_config() noexcept -> USpaceGameLevelConfig* { return level_config.Get(); }
    auto get_level_config() const noexcept -> USpaceGameLevelConfig const* {
        return level_config.Get();
    }
    void set_start_mode(EOrchestratorStartMode mode);
    void set_presentation_enabled(bool enabled);
    void set_level_definition(ml::FLevelDefinition definition) {
        level_definition_ = MoveTemp(definition);
    }
    auto is_presentation_enabled() const noexcept -> bool { return presentation_enabled; }
    void set_time_scale(time_type scale) noexcept;

    /* **************************************** */
    // Simulation state and timing
    /* **************************************** */
    auto frequency_to_tick_period(time_type const frequency) const noexcept -> tick_type;
    auto duration_to_tick_period(time_type const duration) const noexcept -> tick_type;

    auto get_state() const noexcept -> ::ioj::sim::OrchestratorState {
        return level_simulation_.IsSet() ? level_simulation_->get_state()
                                         : ::ioj::sim::OrchestratorState::Uninitialised;
    }
    auto get_completed_ticks() const noexcept -> tick_type {
        return level_simulation_.IsSet() ? level_simulation_->get_clock().get_completed_ticks() : 0;
    }
    auto get_simulation_time() const noexcept -> time_type {
        return level_simulation_.IsSet() ? level_simulation_->get_clock().get_simulation_time()
                                         : 0.0;
    }
    auto get_tick_period() const noexcept -> time_type {
        return 1.0 / simulation_tick_loop.tick_rate;
    }
    auto was_launched_paused() const noexcept -> bool { return launched_paused_; }
    auto get_benchmark_ticks_remaining() const noexcept -> TOptional<tick_type> {
        if (launch_options_.control_context != EPlayerControlContext::Benchmark ||
            !launch_options_.simulated_duration_seconds.IsSet()) {
            return NullOpt;
        }
        auto const end_tick{
            duration_to_tick_period(launch_options_.simulated_duration_seconds.GetValue())};
        auto const completed_ticks{get_completed_ticks()};
        return completed_ticks >= end_tick ? 0 : end_tick - completed_ticks;
    }

    /* **************************************** */
    // Player and combat simulations
    /* **************************************** */
    auto get_player_ship() const -> ATestSpaceShip const*;
    auto get_player_ship_simulation() const noexcept -> ::ioj::sim::player::Sim const*;
    void set_player_ship(ATestSpaceShip& new_player_ship);
    void clear_player_ship();
    auto get_lasers() const noexcept -> ::ioj::sim::lasers::Sim const* {
        return level_simulation_.IsSet() ? &level_simulation_->get_lasers() : nullptr;
    }
    auto get_capital_ships() const noexcept -> ::ioj::sim::capital_ships::Sim const* {
        return level_simulation_.IsSet() ? &level_simulation_->get_capital_ships() : nullptr;
    }
    auto get_fighters() const noexcept -> ::ioj::sim::fighters::Sim const* {
        return level_simulation_.IsSet() ? &level_simulation_->get_fighters() : nullptr;
    }

    /* **************************************** */
    // Simulation services
    /* **************************************** */
    auto get_entity_registry() const noexcept -> ::ioj::sim::EntityRegistry const& {
        check(level_simulation_.IsSet());
        return level_simulation_->get_entity_registry();
    }
    auto get_level_telemetry_manager() const noexcept -> ::ioj::sim::LevelTelemetryManager const& {
        check(level_simulation_.IsSet());
        return level_simulation_->get_level_telemetry_manager();
    }
    auto get_spatial_query_manager() const noexcept -> ::ioj::sim::SpatialQueryManager const& {
        check(level_simulation_.IsSet());
        return level_simulation_->get_spatial_query_manager();
    }
    auto get_mission_manager() const noexcept -> ::ioj::sim::MissionManager const& {
        check(level_simulation_.IsSet());
        return level_simulation_->get_mission_manager();
    }
    auto get_hud_manager() noexcept -> FHUDManager& { return hud_manager; }
    auto get_hud_manager() const noexcept -> FHUDManager const& { return hud_manager; }
    auto get_hud_update_frequencies() const noexcept -> FTestBatchGameUiUpdateFrequencies const& {
        return hud_update_frequencies;
    }

    /* **************************************** */
    // Testing and level preparation
    /* **************************************** */
    void set_end_tick_test_hook(FOrchestratorEndTickTestHook hook);
    void clear_end_tick_test_hook();

    void prepare_level();

    /* **************************************** */
    // Events, results, and presentation
    /* **************************************** */
    static FOnProxyEntitiesBound on_proxy_entities_bound;
    FOnOrchestratorReset on_reset;
    FOnTestMissionCompleted on_mission_completed;
    auto get_mission_definition() -> FLevelMissionDefinition& { return mission_definition; }
    auto get_level_simulation() const -> ::ioj::sim::LevelSim const* {
        return level_simulation_.IsSet() ? &level_simulation_.GetValue() : nullptr;
    }
    auto get_level_presentation() const -> FLevelPresentation const* {
        return level_presentation_.IsSet() ? &level_presentation_.GetValue() : nullptr;
    }
    auto take_finalized_telemetry_report() -> TOptional<FLevelTelemetryReport>;
    auto get_presentation_resources() const -> FLevelPresentationResources {
        return make_presentation_resources();
    }
  protected:
    /* **************************************** */
    // Actor lifecycle
    /* **************************************** */
    void BeginPlay() override;
    void EndPlay(EEndPlayReason::Type end_play_reason) override;

#if WITH_EDITOR
    UFUNCTION(CallInEditor, Category = "Sandbox|Assets")
    void apply_level_config();

    UFUNCTION(CallInEditor, Category = "Sandbox")
    void prepare_level_button();
#endif
  private:
    /* **************************************** */
    // Level initialization
    /* **************************************** */
    auto begin_play() -> bool;
    void load_authored_level();
    auto should_initialise_in_begin_play() const noexcept -> bool;
    auto initialise_simulation(ml::FLevelStartErrors& errors,
                               TOptional<ml::FProxyLevelSimBuild>& proxy_build) -> bool;
    void handle_level_start_failure(FString message);

    /* **************************************** */
    // Mission and telemetry
    /* **************************************** */
    void process_mission_result();
    void process_battle_run_end();
    void handle_telemetry_persisted(FString run_id, FString error);
    void persist_finalized_telemetry_run();

    /* **************************************** */
    // Presentation
    /* **************************************** */
    auto make_presentation_resources() const -> FLevelPresentationResources;
    void refresh_collision_grid_visualization();
    void update_collision_bounds_visualization();

    /* **************************************** */
    // State
    /* **************************************** */
    FOrchestratorEndTickTestHook end_tick_test_hook;

    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (ShowOnlyInnerProperties))
    FFixedTickLoop simulation_tick_loop{};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    EOrchestratorStartMode start_mode{EOrchestratorStartMode::Automatic};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Presentation")
    bool presentation_enabled{true};

    UPROPERTY(EditAnywhere, Category = "Sandbox|Assets")
    TObjectPtr<USpaceGameLevelConfig> level_config{nullptr};

    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Collision")
    TObjectPtr<UCollisionGridVisualizationComponent> collision_grid_visualization{nullptr};

    FHUDManager hud_manager;
    TOptional<::ioj::sim::LevelSim> level_simulation_;
    TOptional<FLevelPresentation> level_presentation_;
    TArray<FTransform> initial_turret_transforms_;
    FLevelTelemetryEnvironment telemetry_environment_;
    TOptional<ml::FLevelDefinition> level_definition_;
    bool launched_paused_{};
    ml::ioj::FLevelLaunchOptions launch_options_{};
    FString level_source_sha256_{};
    ml::ioj::FLevelCollisionHost world_collision_;

    UPROPERTY(EditAnywhere, Category = "Sandbox|UI", meta = (ShowOnlyInnerProperties))
    FTestBatchGameUiUpdateFrequencies hud_update_frequencies{};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestSpaceShip> player_ship{nullptr};
    UPROPERTY(EditAnywhere, Category = "Sandbox", meta = (ShowOnlyInnerProperties))
    FLevelMissionDefinition mission_definition;
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Presentation")
    TObjectPtr<USandboxISMCComponent> laser_instances_;
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Presentation")
    TObjectPtr<USparkRendererComponent> spark_renderer_;
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Presentation")
    TObjectPtr<UInstancedStaticMeshComponent> capital_instances_;
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Presentation")
    TObjectPtr<USandboxISMCComponent> fighter_instances_;
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Presentation")
    TObjectPtr<UInstancedStaticMeshComponent> turret_instances_;
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Presentation")
    TObjectPtr<UInstancedStaticMeshComponent> spinner_instances_;
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Presentation")
    TObjectPtr<UInstancedStaticMeshComponent> soft_target_instances_;

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    bool log_ticks{false};
#endif
};
