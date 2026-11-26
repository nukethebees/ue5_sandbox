#pragma once
#include <ioj/levels/launch_options.h>
#include <ioj/session/mission_launch_request.h>
#include <ioj/sim/levels/level_sim.h>
#include <SpaceGame/levels/LevelCompletion.h>
#include <SpaceGame/levels/LevelLoader.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGamePresentation/presentation/LevelPresentationHost.h>

#include <CoreMinimal.h>
#include <GameFramework/Actor.h>

#include <optional>

#include "TestBatchOrchestrator.generated.h"

class UInstancedStaticMeshComponent;

namespace ioj {
class UCollisionGridVisualizationComponent;
class USandboxISMCComponent;
class USparkRendererComponent;
class ATestSpaceShip;

UCLASS()
class SPACEGAME_API ATestBatchOrchestrator : public AActor {
    GENERATED_BODY()
  public:
    using tick_type = sim::SimTick::value_type;
    using time_type = double;

    /* **************************************** */
    // Lifecycle and simulation control
    /* **************************************** */
    ATestBatchOrchestrator();

    void PostLoad() override;
    void present_level(time_type delta_seconds);
    auto prepare_level(session::MissionLaunchRequest const& mission) -> FLevelLoadResult;
    auto bind_level(session::MissionLaunchRequest const& mission, sim::LevelSim const& level)
        -> bool;
    auto take_frame_delta_seconds() -> std::optional<double>;
    void handle_level_start_failure(FString message);
    void finish_level(profiles::CompletedLevel const& completed,
                      bool scored,
                      bool new_best,
                      bool show_results);
    void start_simulation();
    void pause_simulation();

    /* **************************************** */
    // Level configuration
    /* **************************************** */
    auto set_level_config(USpaceGameLevelConfig& config) -> bool;
    void set_collision_overlays(FCollisionOverlayVisibility visibility);
    auto is_presentation_enabled() const noexcept -> bool {
        return launch_options_.presentation_mode == levels::PresentationMode::Visual;
    }

    /* **************************************** */
    // Simulation state and timing
    /* **************************************** */
    auto get_state() const noexcept -> sim::LevelState {
        return level_simulation_ ? level_simulation_->state() : sim::LevelState::Uninitialised;
    }

    auto get_level_simulation() const -> sim::LevelSim const* { return level_simulation_; }

    auto get_run_ticks_remaining() const noexcept -> TOptional<tick_type> {
        if (!launch_options_.simulated_duration_seconds.has_value() || !level_simulation_) {
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

    /* **************************************** */
    // Events, results, and presentation
    /* **************************************** */
    auto get_hud_manager() noexcept -> FHUDManager& { return presentation_host_.get_hud_manager(); }
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
    void release_level();
    void record_world_delta(UWorld* world, ELevelTick tick_type, float delta_seconds);

    /* **************************************** */
    // Presentation
    /* **************************************** */
    void bind_presentation_components();
    void refresh_collision_grid_visualization();
    auto get_presentation_resources(settings::Settings const& settings) const
        -> FLevelPresentationResources;
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
    FSimpleMulticastDelegate on_simulation_paused;
  private:
    UPROPERTY(EditAnywhere, Category = "Sandbox|Assets")
    TObjectPtr<USpaceGameLevelConfig> level_config{nullptr};

    FCollisionOverlayVisibility collision_overlays_{};
    FLevelPresentationHost presentation_host_;
    sim::LevelSim const* level_simulation_{};
    std::optional<double> frame_delta_seconds_;
    levels::LaunchOptions launch_options_{};

    UPROPERTY(VisibleAnywhere, Category = "Sandbox")
    TObjectPtr<ATestSpaceShip> player_ship{nullptr};
};

} // namespace ioj
