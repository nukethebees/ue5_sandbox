#pragma once
#include <ioj/presentation/level_frame.h>
#include <SpaceGamePresentation/audio/GameAudio.h>
#include <SpaceGamePresentation/presentation/CapitalPresentation.h>
#include <SpaceGamePresentation/presentation/DelayedNiagaraSpawns.h>
#include <SpaceGamePresentation/presentation/FighterPresentation.h>
#include <SpaceGamePresentation/presentation/LaserPresentation.h>
#include <SpaceGamePresentation/presentation/LevelPresentationSettings.h>
#include <SpaceGamePresentation/presentation/PlayerPresentation.h>
#include <SpaceGamePresentation/presentation/SpinnerPresentation.h>
#include <SpaceGamePresentation/presentation/TurretPresentation.h>
#include <SpaceGameRendering/SparkEffects.h>

namespace ioj {
class UCollisionGridVisualizationComponent;

struct SPACEGAMEPRESENTATION_API FLevelPresentationComponents {
    auto is_valid() const -> bool;

    USandboxISMCComponent* lasers{};
    UInstancedStaticMeshComponent* capital_ships{};
    USandboxISMCComponent* fighters{};
    UInstancedStaticMeshComponent* turrets{};
    UInstancedStaticMeshComponent* spinners{};
    USparkRendererComponent* sparks{};
    UInstancedStaticMeshComponent* soft_targets{};
    UCollisionGridVisualizationComponent* collision_grid{};
};

struct SPACEGAMEPRESENTATION_API FLevelPresentation {
    FLevelPresentation(FLevelPresentationComponents const& components,
                       FLevelVisualConfig config,
                       TOptional<FPlayerPresentationResources> player_resources,
                       presentation::LevelFrame const& frame,
                       FGameAudioFacade audio);
    FLevelPresentation(FLevelPresentation const&) = delete;
    FLevelPresentation(FLevelPresentation&&) = delete;
    auto operator=(FLevelPresentation const&) -> FLevelPresentation& = delete;
    auto operator=(FLevelPresentation&&) -> FLevelPresentation& = delete;

    void tick(float dt, presentation::LevelFrame const& frame);
    FLevelVisualConfig const config;
  private:
    FGameAudioFacade audio_;
  public:
    FSparkEffects sparks;
    FLaserPresentation lasers;
    FCapitalPresentation capital_ships;
    FFighterPresentation capital_ship_fighters;
    FTurretPresentation turrets;
    FSpinnerPresentation spinners;
    FDelayedNiagaraSpawns effects;
  private:
    TOptional<FPlayerPresentation> player_;
};

} // namespace ioj
