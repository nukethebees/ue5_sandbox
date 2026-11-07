#pragma once
#include <ioj/levels/launch_options.h>
#include <ioj/sim/levels/level_sim.h>
#include <SpaceGame/entities/ProxyEntityMap.h>
#include <SpaceGame/levels/LevelLoader.h>
#include <SpaceGame/missions/LevelMissionDefinition.h>
#include <SpaceGame/missions/MissionCompletion.h>
#include <SpaceGame/s7/GameSettingsReader.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/system/GameSubsystem.h>
#include <SpaceGamePresentation/presentation/LevelPresentationHost.h>

#include <CoreMinimal.h>
#include <GameFramework/Actor.h>

#include "TestBatchOrchestrator.generated.h"

class ATestSpaceShip;
class UCollisionGridVisualizationComponent;
class UInstancedStaticMeshComponent;
class USandboxISMCComponent;
class USparkRendererComponent;

class ATestBatchOrchestrator;

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
    using tick_type = ioj::sim::SimTick::value_type;
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
    auto get_settings_file() const -> FString const& { return settings_file.FilePath; }
    void set_start_mode(EOrchestratorStartMode mode);
    auto get_start_mode() const noexcept -> EOrchestratorStartMode { return start_mode; }
    void set_presentation_enabled(bool enabled);
    auto get_level_definition() const -> ioj::levels::LevelDefinition const& {
        check(prepared_level_.IsSet() && prepared_level_->level.has_value());
        return *prepared_level_->level;
    }
    auto is_presentation_enabled() const noexcept -> bool { return presentation_enabled; }

    /* **************************************** */
    // Simulation state and timing
    /* **************************************** */
    auto get_state() const noexcept -> ioj::sim::LevelState {
        return level_simulation_.IsSet() ? level_simulation_->state()
                                         : ioj::sim::LevelState::Uninitialised;
    }
    auto get_level_simulation() const -> ioj::sim::LevelSim const* {
        return level_simulation_.IsSet() ? &level_simulation_.GetValue() : nullptr;
    }
    auto was_launched_paused() const noexcept -> bool {
        return launch_options_.launch_mode == ioj::levels::LaunchMode::Paused;
    }
    auto get_run_ticks_remaining() const noexcept -> TOptional<tick_type> {
        if (!launch_options_.simulated_duration_seconds.has_value() || !level_simulation_.IsSet()) {
            return NullOpt;
        }

        auto const& clk{level_simulation_->clock()};
        auto const end_tick{
            clk.duration_to_tick_period(
                   ioj::Duration::make(launch_options_.simulated_duration_seconds.value()))
                .get()};
        auto const completed_ticks{clk.get_completed_ticks().value()};
        return completed_ticks >= end_tick ? 0 : end_tick - completed_ticks;
    }

    /* **************************************** */
    // Player actor
    /* **************************************** */
    auto get_player_ship() const -> ATestSpaceShip const*;
    void set_player_ship(ATestSpaceShip& new_player_ship);
    void clear_player_ship();

    /* **************************************** */
    // Testing and level preparation
    /* **************************************** */

    auto prepare_level(ioj::levels::LevelSource const* source = nullptr) -> bool;
    auto load_level(ioj::levels::LevelSource const& source,
                    ::ioj::levels::ControlContext playerless_control_context =
                        ::ioj::levels::ControlContext::Observer)
        -> std::expected<void, ioj::FLevelLoadErrors>;

    /* **************************************** */
    // Events, results, and presentation
    /* **************************************** */
    auto get_mission_definition() -> FLevelMissionDefinition& { return mission_definition; }
    auto get_hud_manager() noexcept -> ioj::FHUDManager& {
        return presentation_host_.get_hud_manager();
    }
    auto get_level_presentation() const -> FLevelPresentation const* {
        return presentation_host_.get_presentation();
    }
    auto get_presentation_resources() const -> FLevelPresentationResources;
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

    UFUNCTION(CallInEditor,
              Category = "Sandbox|Settings",
              meta = (DisplayName = "Create level settings..."))
    void create_level_settings();
#endif
  private:
    /* **************************************** */
    // Level initialization
    /* **************************************** */
    auto begin_play() -> bool;
    void load_authored_level();
    auto should_initialise_in_begin_play() const noexcept -> bool;
    void handle_level_start_failure(FString message);
    void release_level(ioj::sim::telemetry::RunEndReason reason);

    /* **************************************** */
    // Presentation
    /* **************************************** */
    void bind_presentation_components();
    void refresh_collision_grid_visualization();
    /* **************************************** */
    // State
    /* **************************************** */
    UPROPERTY(VisibleAnywhere, Category = "Sandbox|Collision")
    TObjectPtr<UCollisionGridVisualizationComponent> collision_grid_visualization{nullptr};

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
  public:
    static FOnProxyEntitiesBound on_proxy_entities_bound;
    FOnOrchestratorReset on_reset;
    FSimpleMulticastDelegate on_simulation_paused;
    FOnMissionCompleted on_mission_completed;
  private:
    ioj::settings::TickSettings simulation_tick_settings{};
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

    FLevelPresentationHost presentation_host_;
    TOptional<ioj::sim::LevelSim> level_simulation_;
    TOptional<FLoadedGameSettings> prepared_level_;
    ioj::levels::LaunchOptions launch_options_{};

    UPROPERTY(EditAnywhere, Category = "Sandbox")
    TObjectPtr<ATestSpaceShip> player_ship{nullptr};
    UPROPERTY(Transient)
    FLevelMissionDefinition mission_definition;

#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    bool log_ticks{false};
#endif
};
