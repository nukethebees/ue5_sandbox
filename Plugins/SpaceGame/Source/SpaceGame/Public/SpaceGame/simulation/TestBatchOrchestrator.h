#pragma once
#include <ioj/levels/launch_options.h>
#include <ioj/sim/levels/level_sim.h>
#include <SpaceGame/levels/LevelCompletion.h>
#include <SpaceGame/levels/LevelLoader.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGame/system/GameSubsystem.h>
#include <SpaceGamePresentation/presentation/LevelPresentationHost.h>

#include <CoreMinimal.h>
#include <GameFramework/Actor.h>

#include "TestBatchOrchestrator.generated.h"

class UInstancedStaticMeshComponent;
struct FTestBatchOrchestratorTestAccess;

namespace ioj {
class UCollisionGridVisualizationComponent;
class USandboxISMCComponent;
class USparkRendererComponent;
class ATestSpaceShip;
class ATestBatchOrchestrator;

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
    friend struct ::FTestBatchOrchestratorTestAccess;
  public:
    using tick_type = sim::SimTick::value_type;
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
    void set_level_script(FString path);
    auto get_start_mode() const noexcept -> EOrchestratorStartMode { return start_mode; }
    void set_presentation_enabled(bool enabled);
    auto is_presentation_enabled() const noexcept -> bool {
        return launch_options_.presentation_mode == levels::PresentationMode::Visual;
    }

    /* **************************************** */
    // Simulation state and timing
    /* **************************************** */
    auto get_state() const noexcept -> sim::LevelState {
        return level_simulation_.IsSet() ? level_simulation_->state()
                                         : sim::LevelState::Uninitialised;
    }
    auto get_level_simulation() const -> sim::LevelSim const* {
        return level_simulation_.IsSet() ? &level_simulation_.GetValue() : nullptr;
    }
    auto was_launched_paused() const noexcept -> bool {
        return launch_options_.launch_mode == levels::LaunchMode::Paused;
    }
    auto get_run_ticks_remaining() const noexcept -> TOptional<tick_type> {
        if (!launch_options_.simulated_duration_seconds.has_value() || !level_simulation_.IsSet()) {
            return NullOpt;
        }

        auto const& clk{level_simulation_->clock()};
        auto const end_tick{
            clk.duration_to_tick_period(
                   Duration::make(launch_options_.simulated_duration_seconds.value()))
                .get()};
        auto const completed_ticks{clk.get_completed_ticks().value()};
        return completed_ticks >= end_tick ? 0 : end_tick - completed_ticks;
    }

    /* **************************************** */
    // Player actor
    /* **************************************** */
    auto get_player_ship() const -> ATestSpaceShip const*;
    void clear_player_ship();

    /* **************************************** */
    // Level loading
    /* **************************************** */

    auto load_level(levels::LevelDefinition const& definition,
                    levels::ControlContext playerless_control_context =
                        levels::ControlContext::Observer) -> std::expected<void, FLevelLoadErrors>;

    /* **************************************** */
    // Events, results, and presentation
    /* **************************************** */
    auto get_hud_manager() noexcept -> FHUDManager& { return presentation_host_.get_hud_manager(); }
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

#endif
  private:
    /* **************************************** */
    // Level initialization
    /* **************************************** */
    void load_authored_level();
    auto should_initialise_in_begin_play() const noexcept -> bool;
    void handle_level_start_failure(FString message);
    void release_level(sim::telemetry::RunEndReason reason);
    void report_run_completion();

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
    FOnOrchestratorReset on_reset;
    FSimpleMulticastDelegate on_simulation_paused;
    FOnLevelCompleted on_level_completed;
  private:
    UPROPERTY(EditAnywhere,
              Category = "Sandbox",
              meta = (FilePathFilter = "scm", RelativeToGameDir))
    FFilePath level_script;
    UPROPERTY(EditAnywhere, Category = "Sandbox")
    EOrchestratorStartMode start_mode{EOrchestratorStartMode::Automatic};

    UPROPERTY(EditAnywhere, Category = "Sandbox|Assets")
    TObjectPtr<USpaceGameLevelConfig> level_config{nullptr};

    FLevelPresentationHost presentation_host_;
    TOptional<sim::LevelSim> level_simulation_;
    levels::LaunchOptions launch_options_{};

    UPROPERTY(VisibleAnywhere, Category = "Sandbox")
    TObjectPtr<ATestSpaceShip> player_ship{nullptr};
};

} // namespace ioj
