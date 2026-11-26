#pragma once
#include <ioj/presentation/mission_frame.h>
#include <ioj/presentation/mission_init.h>
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
    void present_level(time_type delta_seconds, presentation::MissionFrame const& frame, sim::LevelSim const& level);
    auto prepare_level(presentation::MissionInit const& mission) -> FLevelLoadResult;
    auto bind_level(presentation::MissionInit const& mission,
                    presentation::MissionFrame const& frame, sim::LevelSim const& level) -> bool;
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
        return presentation_mode_ == levels::PresentationMode::Visual;
    }

    /* **************************************** */
    // Simulation state and timing
    /* **************************************** */
    auto get_state() const noexcept -> sim::LevelState {
        return status_ ? status_->state : sim::LevelState::Uninitialised;
    }

    auto has_level() const -> bool { return status_.has_value(); }
    auto mission_id() const -> FName { return mission_id_; }

    auto get_run_ticks_remaining() const noexcept -> TOptional<tick_type> {
        if (status_ && status_->ticks_remaining) {
            return *status_->ticks_remaining;
        }
        return NullOpt;
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
    auto get_presentation_resources(presentation::MissionInit const& mission) const
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
    std::optional<presentation::MissionStatus> status_;
    FName mission_id_;
    sim::collision::CollisionGridSpec grid_{};
    std::optional<double> frame_delta_seconds_;
    levels::PresentationMode presentation_mode_{levels::PresentationMode::Visual};

    UPROPERTY(VisibleAnywhere, Category = "Sandbox")
    TObjectPtr<ATestSpaceShip> player_ship{nullptr};
};

} // namespace ioj
