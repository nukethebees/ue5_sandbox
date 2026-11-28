#pragma once
#include <ioj/presentation/level_frame.h>
#include <ioj/presentation/level_init.h>
#include <SpaceGame/levels/LevelCompletion.h>
#include <SpaceGame/levels/LevelLoader.h>
#include <SpaceGame/simulation/SpaceGameLevelConfig.h>
#include <SpaceGamePresentation/presentation/LevelPresentationHost.h>

#include <CoreMinimal.h>
#include <GameFramework/Actor.h>

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
    using time_type = double;

    /* **************************************** */
    // Lifecycle and simulation control
    /* **************************************** */
    ATestBatchOrchestrator();

    void PostLoad() override;
    void present_level(time_type delta_seconds, presentation::LevelFrame const& frame);
    auto initialize_level(presentation::LevelInit const& level,
                          presentation::LevelFrame const& frame) -> FLevelLoadResult;
    void release_level();
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
    auto get_state() const noexcept -> sim::LevelState { return status_.state; }

    auto has_level() const -> bool { return status_.state != sim::LevelState::Uninitialised; }
    auto status() const -> presentation::LevelStatus const& { return status_; }

    /* **************************************** */
    // Player actor
    /* **************************************** */
    auto get_player_ship() const -> ATestSpaceShip const*;
    void clear_player_ship();

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
    UPROPERTY(EditAnywhere, Category = "Sandbox|Assets")
    TObjectPtr<USpaceGameLevelConfig> level_config{nullptr};

    FCollisionOverlayVisibility collision_overlays_{};
    FLevelPresentationHost presentation_host_;
    presentation::LevelStatus status_{};
    sim::collision::CollisionGridSpec grid_{};
    levels::PresentationMode presentation_mode_{levels::PresentationMode::Visual};

    UPROPERTY(VisibleAnywhere, Category = "Sandbox")
    TObjectPtr<ATestSpaceShip> player_ship{nullptr};
};

} // namespace ioj
