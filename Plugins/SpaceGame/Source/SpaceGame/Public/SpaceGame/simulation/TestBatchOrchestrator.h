#pragma once
#include <ioj/sim/levels/level_sim.h>
#include <SpaceGame/entities/ProxyEntityMap.h>
#include <SpaceGame/missions/LevelMissionDefinition.h>
#include <SpaceGame/missions/MissionCompletion.h>
#include <SpaceGame/simulation/LevelCollisionHost.h>
#include <SpaceGame/simulation/LevelSimulationBuilder.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/system/GameSubsystem.h>
#include <SpaceGame/telemetry/LevelTelemetryReport.h>
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
    using tick_type = ioj::sim::SimTick;
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
    auto set_level_config(USpaceGameLevelConfig& config) -> bool;
    auto get_level_config() noexcept -> USpaceGameLevelConfig* { return level_config.Get(); }
    auto get_level_config() const noexcept -> USpaceGameLevelConfig const* {
        return level_config.Get();
    }
    void set_start_mode(EOrchestratorStartMode mode);
    auto get_start_mode() const noexcept -> EOrchestratorStartMode { return start_mode; }
    void set_presentation_enabled(bool enabled);
    void set_collision_grid_override(FVector3f level_size, FVector3f cell_size);
    void clear_collision_grid_override();
    void set_level_definition(ioj::levels::LevelDefinition definition) {
        settings_prepared_ = false;
        level_definition_ = MoveTemp(definition);
    }
    auto is_presentation_enabled() const noexcept -> bool { return presentation_enabled; }
    void set_time_scale(ioj::PositiveFinite<time_type> scale) noexcept;

    /* **************************************** */
    // Simulation state and timing
    /* **************************************** */
    auto frequency_to_tick_period(ioj::Frequency frequency) const noexcept -> tick_type;
    auto duration_to_tick_period(ioj::Duration duration) const noexcept -> tick_type;

    auto get_state() const noexcept -> ioj::sim::OrchestratorState {
        return level_simulation_.IsSet() ? level_simulation_->get_state()
                                         : ioj::sim::OrchestratorState::Uninitialised;
    }
    auto get_completed_ticks() const noexcept -> tick_type {
        return level_simulation_.IsSet() ? level_simulation_->get_clock().get_completed_ticks() : 0;
    }
    auto get_simulation_time() const noexcept -> time_type {
        return level_simulation_.IsSet() ? level_simulation_->get_clock().get_simulation_time()
                                         : 0.0;
    }
    auto get_tick_period() const noexcept -> time_type {
        return level_simulation_.IsSet() ? level_simulation_->get_clock().get_tick_period()
                                         : simulation_tick_loop.get_tick_period();
    }
    auto was_launched_paused() const noexcept -> bool { return launched_paused_; }
    auto get_benchmark_ticks_remaining() const noexcept -> TOptional<tick_type> {
        if (launch_options_.control_context != EPlayerControlContext::Benchmark ||
            !launch_options_.simulated_duration_seconds.IsSet()) {
            return NullOpt;
        }
        auto const end_tick{duration_to_tick_period(
            ioj::Duration::make(launch_options_.simulated_duration_seconds.GetValue()))};
        auto const completed_ticks{get_completed_ticks()};
        return completed_ticks >= end_tick ? 0 : end_tick - completed_ticks;
    }

    /* **************************************** */
    // Player and combat simulations
    /* **************************************** */
    auto get_player_ship() const -> ATestSpaceShip const*;
    auto get_player_ship_simulation() const noexcept -> ioj::sim::player::Sim const*;
    void set_player_ship(ATestSpaceShip& new_player_ship);
    void clear_player_ship();
    auto get_lasers() const noexcept -> ioj::sim::lasers::Sim const* {
        return level_simulation_.IsSet() ? &level_simulation_->get_lasers() : nullptr;
    }
    auto get_capital_ships() const noexcept -> ioj::sim::capital_ships::Sim const* {
        return level_simulation_.IsSet() ? &level_simulation_->get_capital_ships() : nullptr;
    }
    auto get_fighters() const noexcept -> ioj::sim::fighters::Sim const* {
        return level_simulation_.IsSet() ? &level_simulation_->get_fighters() : nullptr;
    }

    /* **************************************** */
    // Simulation services
    /* **************************************** */
    auto get_entity_ledger() const noexcept -> ioj::sim::EntityLedger const& {
        check(level_simulation_.IsSet());
        return level_simulation_->get_entity_ledger();
    }
    auto get_level_telemetry_manager() const noexcept -> ioj::sim::LevelTelemetryManager const& {
        check(level_simulation_.IsSet());
        return level_simulation_->get_level_telemetry_manager();
    }
    auto get_spatial_query_manager() const noexcept -> ioj::sim::SpatialQueryManager const& {
        check(level_simulation_.IsSet());
        return level_simulation_->get_spatial_query_manager();
    }
    auto get_level_id() const -> std::string const& {
        check(level_simulation_.IsSet());
        return level_simulation_->get_level_id();
    }
    auto get_mission() const noexcept -> ioj::sim::Mission const& {
        check(level_simulation_.IsSet());
        return level_simulation_->get_mission();
    }
    auto get_hud_manager() noexcept -> FHUDManager& { return hud_manager; }
    auto get_hud_manager() const noexcept -> FHUDManager const& { return hud_manager; }
    auto get_hud_update_frequencies() const noexcept -> ioj::sim::HudSettings const& {
        return hud_update_frequencies;
    }

    /* **************************************** */
    // Testing and level preparation
    /* **************************************** */
    void set_end_tick_test_hook(FOrchestratorEndTickTestHook hook);
    void clear_end_tick_test_hook();

    auto prepare_level() -> bool;

    /* **************************************** */
    // Events, results, and presentation
    /* **************************************** */
    static FOnProxyEntitiesBound on_proxy_entities_bound;
    FOnOrchestratorReset on_reset;
    FOnTestMissionCompleted on_mission_completed;
    auto get_mission_definition() -> FLevelMissionDefinition& { return mission_definition; }
    auto get_level_simulation() const -> ioj::sim::LevelSim const* {
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
    auto initialise_simulation(USpaceGameLevelConfig const& effective_config,
                               ioj::FLevelStartErrors& errors,
                               TOptional<ioj::FProxyLevelSimBuild>& proxy_build,
                               ioj::sim::collision::GridGeometry& grid_geometry) -> bool;
    void handle_level_start_failure(FString message);

    /* **************************************** */
    // Mission and telemetry
    /* **************************************** */
    void process_mission_result();
    void handle_telemetry_persisted(FString run_id, FString error);
    void persist_finalized_telemetry_run();

    /* **************************************** */
    // Presentation
    /* **************************************** */
    auto make_presentation_resources() const -> FLevelPresentationResources;
    void refresh_collision_grid_visualization();
    [[nodiscard]] auto apply_collision_grid_overrides(FCollisionGridConfig const& source) const
        -> FCollisionGridConfig;
    void update_collision_bounds_visualization();

    /* **************************************** */
    // State
    /* **************************************** */
    FOrchestratorEndTickTestHook end_tick_test_hook;

    ioj::sim::FixedTickLoop simulation_tick_loop{};
    TOptional<ioj::PositiveFinite<time_type>> time_scale_override_;
    bool settings_prepared_{};

    UPROPERTY(EditAnywhere,
              Category = "Sandbox|Settings",
              meta = (FilePathFilter = "scm", RelativeToGameDir))
    FFilePath settings_file{TEXT("LevelScripts/Settings/base.scm")};
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    EOrchestratorStartMode start_mode{EOrchestratorStartMode::Automatic};
    UPROPERTY(EditAnywhere, Category = "Sandbox|Presentation")
    bool presentation_enabled{true};

    UPROPERTY(EditAnywhere, Category = "Sandbox|Assets")
    TObjectPtr<USpaceGameLevelConfig> level_config{nullptr};

    bool use_collision_grid_override{};

    FVector3f level_size_override{FVector3f::ZeroVector};

    FVector3f grid_cell_size_override{FVector3f::ZeroVector};

    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Collision")
    TObjectPtr<UCollisionGridVisualizationComponent> collision_grid_visualization{nullptr};

    FHUDManager hud_manager;
    TOptional<ioj::sim::LevelSim> level_simulation_;
    TOptional<FLevelPresentation> level_presentation_;
    TArray<FTransform> initial_turret_transforms_;
    FLevelTelemetryEnvironment telemetry_environment_;
    TOptional<ioj::levels::LevelDefinition> level_definition_;
    bool launched_paused_{};
    ioj::FLevelLaunchOptions launch_options_{};
    FString level_source_{};
    FString settings_source_{};
    ioj::FLevelCollisionHost world_collision_;

    ioj::sim::HudSettings hud_update_frequencies{};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestSpaceShip> player_ship{nullptr};
    UPROPERTY(Transient)
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
