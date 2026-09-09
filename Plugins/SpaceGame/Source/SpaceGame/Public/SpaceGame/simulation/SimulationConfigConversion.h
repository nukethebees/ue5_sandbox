#pragma once
#include <SpaceGamePresentation/presentation/LevelActorSettings.h>
#include <SpaceGameSimulation/simulation/LevelSimulationConfig.h>
auto SPACEGAME_API make_simulation_config(FLaserWeaponConfig const& source)
    -> FSimulationLaserWeaponConfig;
auto SPACEGAME_API make_simulation_config(FPlayerShipConfig const& source)
    -> FPlayerSimulationConfig;
auto SPACEGAME_API make_simulation_config(FLaserProjectileConfig const& source)
    -> FLaserSimulationConfig;
auto SPACEGAME_API make_simulation_config(FCapitalShipConfig const& source)
    -> FCapitalSimulationConfig;
auto SPACEGAME_API make_simulation_config(FFighterConfig const& source) -> FFighterSimulationConfig;
auto SPACEGAME_API make_simulation_config(FTurretConfig const& source) -> FTurretSimulationConfig;
auto SPACEGAME_API make_simulation_config(FTubeSpinnerConfig const& source)
    -> FSpinnerSimulationConfig;
