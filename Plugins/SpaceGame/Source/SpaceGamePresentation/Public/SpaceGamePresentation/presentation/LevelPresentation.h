#pragma once
#include <SpaceGamePresentation/presentation/CapitalPresentation.h>
#include <SpaceGamePresentation/presentation/DelayedNiagaraSpawns.h>
#include <SpaceGamePresentation/presentation/FighterPresentation.h>
#include <SpaceGamePresentation/presentation/LaserPresentation.h>
#include <SpaceGamePresentation/presentation/LevelPresentationSettings.h>
#include <SpaceGamePresentation/presentation/PlayerPresentation.h>
#include <SpaceGamePresentation/presentation/SpinnerPresentation.h>
#include <SpaceGamePresentation/presentation/TurretPresentation.h>
#include <SpaceGameRendering/SparkEffects.h>

#include <SpaceGameSimulation/simulation/LevelReadView.h>

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
                       FLevelReadView const& view,
                       TArray<FTransform> turret_transforms);
    FLevelPresentation(FLevelPresentation const&) = delete;
    FLevelPresentation(FLevelPresentation&&) = delete;
    auto operator=(FLevelPresentation const&) -> FLevelPresentation& = delete;
    auto operator=(FLevelPresentation&&) -> FLevelPresentation& = delete;

    void tick(float dt, FLevelReadView const& view);
    auto get_tick_count() const -> uint64 { return tick_count_; }
    auto get_last_completed_tick() const -> uint64 { return last_completed_tick_; }
  private:
    FLevelVisualConfig config_;
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
    uint64 last_frame_sequence_{};
    uint64 tick_count_{};
    uint64 last_completed_tick_{};
    void update_views(FLevelReadView const& view, bool consume_changes);
};
