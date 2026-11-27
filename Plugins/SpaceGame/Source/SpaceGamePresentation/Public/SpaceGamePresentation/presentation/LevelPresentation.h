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
struct SPACEGAMEPRESENTATION_API FLevelPresentationResources {
    USandboxISMCComponent* lasers{nullptr};
    UInstancedStaticMeshComponent* capital_ships{nullptr};
    USandboxISMCComponent* fighters{nullptr};
    UInstancedStaticMeshComponent* turrets{nullptr};
    UInstancedStaticMeshComponent* spinners{nullptr};
    USparkRendererComponent* sparks{nullptr};
    FLevelVisualConfig config;
    TOptional<FPlayerPresentationResources> player;
    auto is_valid() const -> bool;
};

struct SPACEGAMEPRESENTATION_API FLevelPresentation {
    FLevelPresentation(FLevelPresentationResources const& resources,
                       presentation::LevelFrame const& frame,
                       FGameAudioFacade audio);
    FLevelPresentation(FLevelPresentation const&) = delete;
    FLevelPresentation(FLevelPresentation&&) = delete;
    auto operator=(FLevelPresentation const&) -> FLevelPresentation& = delete;
    auto operator=(FLevelPresentation&&) -> FLevelPresentation& = delete;

    void tick(float dt, presentation::LevelFrame const& frame);
  private:
    FLevelVisualConfig config_;
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
