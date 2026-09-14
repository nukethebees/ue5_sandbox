#pragma once
#include <ioj/sim/sim_config.h>
#include <SpaceGamePresentation/presentation/LevelActorSettings.h>
auto SPACEGAME_API make_simulation_config(FLaserWeaponConfig const& source)
    -> ::ioj::sim::LaserWeaponSimConfig;
auto SPACEGAME_API make_simulation_config(FPlayerShipConfig const& source)
    -> ::ioj::sim::PlayerSimConfig;
auto SPACEGAME_API make_simulation_config(FLaserProjectileConfig const& source)
    -> ::ioj::sim::LaserSimConfig;
auto SPACEGAME_API make_simulation_config(FCapitalShipConfig const& source)
    -> ::ioj::sim::CapitalShipSimConfig;
auto SPACEGAME_API make_simulation_config(FFighterConfig const& source)
    -> ::ioj::sim::FighterSimConfig;
auto SPACEGAME_API make_simulation_config(FTurretConfig const& source)
    -> ::ioj::sim::TurretSimConfig;
auto SPACEGAME_API make_simulation_config(FTubeSpinnerConfig const& source)
    -> ::ioj::sim::SpinnerSimConfig;
