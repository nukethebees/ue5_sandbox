#pragma once
#include <SpaceGame/presentation/CapitalPresentation.h>
#include <SpaceGame/presentation/DelayedNiagaraSpawns.h>
#include <SpaceGame/presentation/FighterPresentation.h>
#include <SpaceGame/presentation/LaserPresentation.h>
#include <SpaceGame/presentation/LevelPresentationSettings.h>
#include <SpaceGame/presentation/SpinnerPresentation.h>
#include <SpaceGame/presentation/TurretPresentation.h>

class ATestSpaceShip;
struct FLevelSimulation;

struct SPACEGAME_API FLevelPresentationResources {
    USandboxISMCComponent* lasers{nullptr};
    UInstancedStaticMeshComponent* capital_ships{nullptr};
    UInstancedStaticMeshComponent* fighters{nullptr};
    UInstancedStaticMeshComponent* turrets{nullptr};
    UInstancedStaticMeshComponent* spinners{nullptr};
    USpaceGameLevelConfig const* config{nullptr};
    TWeakObjectPtr<ATestSpaceShip> player;
    FLevelPresentationSettings settings;
    auto is_valid() const -> bool;
};

struct SPACEGAME_API FLevelPresentation {
    FLevelPresentation(FLevelPresentationResources const& resources,
                       FLevelSimulation& simulation,
                       TArray<FTransform> turret_transforms);
    ~FLevelPresentation();
    void update_visual_data(float dt);
    void commit_visual_data(float dt);
    void handle_player_death();

    FLaserPresentation lasers;
    FCapitalPresentation capital_ships;
    FFighterPresentation capital_ship_fighters;
    FTurretPresentation turrets;
    FSpinnerPresentation spinners;
    FDelayedNiagaraSpawns effects;
  private:
    TWeakObjectPtr<ATestSpaceShip> player_;
};
