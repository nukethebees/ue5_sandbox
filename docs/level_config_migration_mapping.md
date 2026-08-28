# Level configuration migration mapping

The migration used the actor-selected asset as the canonical value when it conflicted with the
asset referenced by `DA_default_simulation_config`. Null map-level class overrides did not replace
non-null classes from `DA_default_test_simulation_config`.

| Old asset/property | New `USpaceGameLevelConfig` field |
|---|---|
| `UTestSimulationConfig.player_controller_class` | `classes.player_controller_class` |
| `UTestSimulationConfig.actor_classes.*` | `classes.*` |
| `USimulationConfig.player_ship_config` | `player_ship` |
| `UTestSpaceShipData.*` | `player_ship.*` (same terminal property name) |
| `UTestSpaceShipData.laser_damage` | `player_ship.laser.damage` |
| `UTestSpaceShipData.laser_speed` | `player_ship.laser.projectile_speed` |
| `UTestSpaceShipData.laser_max_distance` | `player_ship.laser.max_distance` |
| `UTestSpaceShipData.laser_firing_period` | `player_ship.laser.fire_cooldown` |
| `USimulationConfig.lasers_config` | `laser_projectiles` |
| `UTestLasersConfig.*` | `laser_projectiles.*` (same terminal property name) |
| `USimulationConfig.capital_ships_config` | `capital_ships` |
| `UTestCapitalShipsConfig.*` | `capital_ships.*` (same terminal property name) |
| `UTestCapitalShipsConfig.main_explosion_delay_mode` | `capital_ships.main_explosion_delay_mode` (equivalent unified enum) |
| `USimulationConfig.capital_ship_fighters_config` | `fighters` |
| `UTestCapitalShipFightersConfig.*` | `fighters.*` (same terminal property name) |
| `UTestCapitalShipFightersConfig.laser_damage` | `fighters.laser.damage` |
| `UTestCapitalShipFightersConfig.laser_speed` | `fighters.laser.projectile_speed` |
| `UTestCapitalShipFightersConfig.laser_max_distance` | `fighters.laser.max_distance` |
| `UTestCapitalShipFightersConfig.fire_cooldown` | `fighters.laser.fire_cooldown` |
| `USimulationConfig.static_turrets_config` | `turrets` |
| `UTestStaticTurretsConfig.*` | `turrets.*` (same terminal property name) |
| `UTestStaticTurretsConfig.laser_damage` | `turrets.laser.damage` |
| `UTestStaticTurretsConfig.laser_speed` | `turrets.laser.projectile_speed` |
| `UTestStaticTurretsConfig.laser_max_distance` | `turrets.laser.max_distance` |
| `UTestStaticTurretsConfig.attack_cooldown` | `turrets.laser.fire_cooldown` |
| `USimulationConfig.tube_spinners_config` | `tube_spinners` |
| `UTestTubeSpinnersConfig.*` | `tube_spinners.*` (same terminal property name) |
| `UTestTubeSpinnersConfig.laser_damage` | `tube_spinners.laser.damage` |
| `UTestTubeSpinnersConfig.laser_speed` | `tube_spinners.laser.projectile_speed` |
| `UTestTubeSpinnersConfig.laser_max_distance` | `tube_spinners.laser.max_distance` |
| `UTestTubeSpinnersConfig.attack_cooldown` | `tube_spinners.laser.fire_cooldown` |

The `impossible_survival` turret override contained a null `team_visual_data` reference. The
migration preserved and reported it instead of substituting another scenario's value.
