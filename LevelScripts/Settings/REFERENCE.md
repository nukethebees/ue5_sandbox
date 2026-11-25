# Scheme settings reference

## `settings`

| Member | Type | Enum values |
| --- | --- | --- |
| `:entity-geometry` | `EntityGeometry` | |
| `:tick` | `TickSettings` | |
| `:player-ship` | `PlayerSimConfig` | |
| `:laser-projectiles` | `LaserSimConfig` | |
| `:capital-ships` | `CapitalShipSimConfig` | |
| `:fighters` | `FighterSimConfig` | |
| `:turrets` | `TurretSimConfig` | |
| `:tube-spinners` | `SpinnerSimConfig` | |
| `:collision-grid` | `OverlapResponseConfig` | |
| `:hud` | `HudSettings` | |
| `:presentation` | `Presentation` | |
| `:starfield` | `Settings` | |

## `application`

| Member | Type | Enum values |
| --- | --- | --- |
| `:unlock-all-missions` | `bool` | |
| `:start-levels-paused` | `bool` | |
| `:print-save-data` | `bool` | |
| `:frame-log-footer` | `bool` | |

## `user-settings`

| Member | Type | Enum values |
| --- | --- | --- |
| `:resolution` | `ScreenResolution` | |
| `:window-mode` | `GameWindowMode` | `'windowed` `'borderless` `'fullscreen` |
| `:vsync` | `bool` | |
| `:frame-rate-limit` | `float` | |
| `:resolution-scale` | `float` | |
| `:view-distance-quality` | `GameQualityLevel` | `'low` `'medium` `'high` `'epic` |
| `:aa-method` | `GameAntiAliasingMethod` | `'off` `'fxaa` `'taa` `'tsr` `'smaa` |
| `:aa-quality` | `GameQualityLevel` | `'low` `'medium` `'high` `'epic` |
| `:shadow-quality` | `GameQualityLevel` | `'low` `'medium` `'high` `'epic` |
| `:global-illumination-quality` | `GameQualityLevel` | `'low` `'medium` `'high` `'epic` |
| `:reflections-quality` | `GameQualityLevel` | `'low` `'medium` `'high` `'epic` |
| `:post-processing-quality` | `GameQualityLevel` | `'low` `'medium` `'high` `'epic` |
| `:texture-quality` | `GameQualityLevel` | `'low` `'medium` `'high` `'epic` |
| `:effects-quality` | `GameQualityLevel` | `'low` `'medium` `'high` `'epic` |
| `:shading-quality` | `GameQualityLevel` | `'low` `'medium` `'high` `'epic` |
| `:bloom` | `bool` | |
| `:motion-blur` | `bool` | |
| `:master-volume` | `float` | |
| `:music-volume` | `float` | |
| `:sfx-volume` | `float` | |
| `:ui-volume` | `float` | |
| `:mouse-turn-sensitivity` | `float` | |
| `:gamepad-turn-sensitivity` | `float` | |
| `:gamepad-turn-dead-zone` | `float` | |
| `:invert-mouse-pitch` | `bool` | |
| `:invert-mouse-yaw` | `bool` | |
| `:invert-gamepad-pitch` | `bool` | |
| `:invert-gamepad-yaw` | `bool` | |

## `entity-geometry`

| Member | Type | Enum values |
| --- | --- | --- |
| `:player-ship` | `MeshBounds` | |
| `:capital-ship` | `MeshBounds` | |
| `:fighter` | `MeshBounds` | |
| `:turret` | `MeshBounds` | |
| `:tube-spinner` | `MeshBounds` | |
| `:fighter-launch-slots` | `geometry_transforms` | |
| `:fighter-fire-point-distance` | `float` | |
| `:turret-fire-point` | `native_vector_3f` | |
| `:spinner-fire-points` | `geometry_transforms` | |
| `:player-body` | `AttachmentTransform` | |
| `:player-left-socket` | `AttachmentTransform` | |
| `:player-middle-socket` | `AttachmentTransform` | |
| `:player-right-socket` | `AttachmentTransform` | |

## `tick`

| Member | Type | Enum values |
| --- | --- | --- |
| `:tick-rate` | `settings_frequency` | |
| `:time-scale` | `settings_positive_double` | |

## `player-ship`

| Member | Type | Enum values |
| --- | --- | --- |
| `:thrust-energy-max` | `settings_positive_float` | |
| `:cruise-speed` | `settings_nonnegative_float` | |
| `:boost-speed` | `settings_nonnegative_float` | |
| `:brake-speed` | `settings_nonnegative_float` | |
| `:brake-deceleration` | `settings_nonnegative_float` | |
| `:emergency-brake-deceleration` | `settings_nonnegative_float` | |
| `:turn-rate` | `settings_nonnegative_float` | |
| `:roll-level-delay` | `settings_nonnegative_float` | |
| `:roll-level-rate` | `settings_nonnegative_float` | |
| `:response-settling-time` | `settings_positive_float` | |
| `:response-damping-ratio` | `settings_unit_open_float` | |
| `:boost-energy-drain` | `settings_nonnegative_float` | |
| `:brake-energy-drain` | `settings_nonnegative_float` | |
| `:energy-recharge` | `settings_nonnegative_float` | |
| `:pitch-angle-max` | `float` | |
| `:pitch-speed` | `float` | |
| `:yaw-angle-max` | `float` | |
| `:yaw-speed` | `float` | |
| `:turn-bank-angle-max` | `float` | |
| `:turn-bank-speed` | `float` | |
| `:laser` | `LaserWeaponSimConfig` | |
| `:health` | `settings_positive_health` | |
| `:laser-mode` | `ShipLaserMode` | `'single` `'double` `'hyper` |
| `:laser-trigger-mode` | `ShipTriggerMode` | `'single` `'burst3` `'full-auto` |

## `laser-projectiles`

| Member | Type | Enum values |
| --- | --- | --- |
| `:n-preallocated-instances` | `settings_nonnegative_int` | |

## `capital-ships`

| Member | Type | Enum values |
| --- | --- | --- |
| `:spawn-delay` | `float` | |
| `:max-health` | `settings_positive_health` | |

## `fighters`

| Member | Type | Enum values |
| --- | --- | --- |
| `:max-live-auto-spawned-fighters` | `settings_nonnegative_int` | |
| `:fire-dot-product-threshold` | `settings_dot_product` | |
| `:attack-distance-band` | `AttackDistanceBand` | |
| `:speed` | `float` | |
| `:turn-speed-unitless` | `float` | |
| `:avoidance-clear-update-frequency` | `settings_positive_float` | |
| `:avoidance-update-frequency` | `settings_positive_float` | |
| `:avoidance-active-update-frequency` | `settings_positive_float` | |
| `:avoidance-immediate-update-frequency` | `settings_positive_float` | |
| `:avoidance-lookahead-time` | `settings_nonnegative_float` | |
| `:avoidance-clearance-buffer` | `settings_nonnegative_float` | |
| `:separation-radius` | `settings_positive_float` | |
| `:separation-strength` | `settings_nonnegative_float` | |
| `:steering-memory-duration` | `settings_nonnegative_float` | |
| `:dense-traffic-neighbour-threshold` | `settings_neighbour_threshold` | |
| `:laser` | `LaserWeaponSimConfig` | |
| `:health` | `settings_positive_health` | |
| `:attack-retry-cooldown` | `settings_nonnegative_float` | |
| `:los-check-buffer` | `float` | |

## `turrets`

| Member | Type | Enum values |
| --- | --- | --- |
| `:detection-radius` | `float` | |
| `:target-refresh-frequency` | `settings_positive_float` | |
| `:laser` | `LaserWeaponSimConfig` | |
| `:max-health` | `settings_positive_health` | |

## `tube-spinners`

| Member | Type | Enum values |
| --- | --- | --- |
| `:yaw-rotation-speed-degrees` | `float` | |
| `:laser` | `LaserWeaponSimConfig` | |

## `collision-grid`

| Member | Type | Enum values |
| --- | --- | --- |
| `:damage-per-overlap-detection` | `settings_positive_int` | |
| `:level-size` | `native_vector_3f` | |
| `:cell-size` | `native_vector_3f` | |

## `hud`

| Member | Type | Enum values |
| --- | --- | --- |
| `:player-status-update-period` | `settings_nonnegative_float` | |
| `:entity-count-update-period` | `settings_nonnegative_float` | |

## `presentation`

| Member | Type | Enum values |
| --- | --- | --- |
| `:player-ship` | `PlayerPresentation` | |
| `:laser-projectiles` | `LaserPresentation` | |
| `:capital-ships` | `CapitalPresentation` | |
| `:turrets` | `TurretPresentation` | |
| `:entity-overlay` | `EntityOverlay` | |
| `:radar` | `Radar` | |

## `starfield`

| Member | Type | Enum values |
| --- | --- | --- |
| `:generation` | `GenerationSettings` | |
| `:star` | `StarSettings` | |
| `:galactic-band` | `GalacticBandSettings` | |
| `:stellar-cluster` | `StellarClusterSettings` | |
| `:dust-lane` | `DustLaneSettings` | |
| `:bright-star` | `BrightStarSettings` | |
| `:twinkle` | `TwinkleSettings` | |
| `:galactic-haze` | `GalacticHazeSettings` | |
| `:galactic-core` | `GalacticCoreSettings` | |
| `:nebular-knot` | `NebularKnotSettings` | |

## `resolution`

| Member | Type | Enum values |
| --- | --- | --- |
| `width` | `std::int32_t` | |
| `height` | `std::int32_t` | |

## `mesh-bounds`

| Member | Type | Enum values |
| --- | --- | --- |
| `:centre` | `native_vector_3f` | |
| `:half-extents` | `native_vector_3f` | |

## `attachment-transform`

| Member | Type | Enum values |
| --- | --- | --- |
| `:location` | `native_vector3d` | |
| `:rotation` | `geometry_quaternion` | |
| `:scale` | `native_vector3d` | |

## `laser`

| Member | Type | Enum values |
| --- | --- | --- |
| `:damage` | `std::int32_t` | |
| `:projectile-speed` | `settings_positive_float` | |
| `:max-distance` | `settings_positive_float` | |
| `:fire-cooldown` | `settings_nonnegative_float` | |

## `attack-distance-band`

| Member | Type | Enum values |
| --- | --- | --- |
| `:minimum-ratio` | `float` | |
| `:desired-ratio` | `float` | |
| `:maximum-ratio` | `float` | |

## `player-ship-presentation`

| Member | Type | Enum values |
| --- | --- | --- |
| `:boost-effect-colour-intensity` | `float` | |

## `laser-projectiles-presentation`

| Member | Type | Enum values |
| --- | --- | --- |
| `:min-cull-distance` | `float` | |
| `:max-cull-distance` | `float` | |

## `capital-ships-presentation`

| Member | Type | Enum values |
| --- | --- | --- |
| `:n-small-explosions` | `std::int32_t` | |
| `:time-between-explosions` | `float` | |
| `:min-small-explosion-range` | `native_vector_3f` | |
| `:max-small-explosion-range` | `native_vector_3f` | |
| `:large-explosion-delay` | `float` | |
| `:proxy-arrow-size` | `float` | |

## `turrets-presentation`

| Member | Type | Enum values |
| --- | --- | --- |
| `:death-effect-scale` | `float` | |

## `entity-overlay`

| Member | Type | Enum values |
| --- | --- | --- |
| `:enabled` | `bool` | |
| `:maximum-range` | `float` | |
| `:bar-size-pixels` | `settings_vector2d` | |
| `:screen-offset-pixels` | `settings_vector2d` | |
| `:minimum-bar-scale` | `float` | |
| `:maximum-bar-scale` | `float` | |
| `:inset-pixels` | `float` | |
| `:maximum-inset-height-ratio` | `float` | |
| `:objective-frame-pixels` | `float` | |
| `:objective-bar-height-scale` | `float` | |
| `:screen-edge-padding-pixels` | `float` | |
| `:soft-target` | `SoftTarget` | |

## `radar`

| Member | Type | Enum values |
| --- | --- | --- |
| `:enabled` | `bool` | |
| `:combat-range` | `float` | |
| `:tactical-range` | `float` | |
| `:maximum-range` | `float` | |
| `:combat-display-radius` | `float` | |
| `:grid-opacity` | `float` | |
| `:core-cell-radius` | `float` | |
| `:combat-cell-radius` | `float` | |
| `:tactical-cell-radius` | `float` | |
| `:strategic-cell-radius` | `float` | |
| `:glyph-size-scale` | `float` | |
| `:objective-size-multiplier` | `float` | |
| `:objective-ring-padding-pixels` | `float` | |
| `:objective-ring-thickness-pixels` | `float` | |

## `generation`

| Member | Type | Enum values |
| --- | --- | --- |
| `:star-count` | `native_int32` | |
| `:random-seed` | `native_int32` | |

## `star`

| Member | Type | Enum values |
| --- | --- | --- |
| `:scale` | `float` | |
| `:size-multiplier` | `float` | |
| `:global-brightness` | `float` | |
| `:colour-variation-strength` | `float` | |
| `:parallax-strength` | `float` | |

## `galactic-band`

| Member | Type | Enum values |
| --- | --- | --- |
| `:strength` | `float` | |
| `:width-degrees` | `float` | |

## `stellar-cluster`

| Member | Type | Enum values |
| --- | --- | --- |
| `:strength` | `float` | |
| `:width-degrees` | `float` | |

## `dust-lane`

| Member | Type | Enum values |
| --- | --- | --- |
| `:strength` | `float` | |
| `:width-degrees` | `float` | |
| `:irregularity` | `float` | |

## `bright-star`

| Member | Type | Enum values |
| --- | --- | --- |
| `:fraction` | `float` | |
| `:size-multiplier` | `float` | |
| `:brightness-multiplier` | `float` | |
| `:shape-strength` | `float` | |

## `twinkle`

| Member | Type | Enum values |
| --- | --- | --- |
| `:strength` | `float` | |
| `:speed` | `float` | |

## `galactic-haze`

| Member | Type | Enum values |
| --- | --- | --- |
| `:strength` | `float` | |
| `:width-degrees` | `float` | |

## `galactic-core`

| Member | Type | Enum values |
| --- | --- | --- |
| `:strength` | `float` | |
| `:width-degrees` | `float` | |

## `nebular-knot`

| Member | Type | Enum values |
| --- | --- | --- |
| `:strength` | `float` | |
| `:size-degrees` | `float` | |

## `soft-target`

| Member | Type | Enum values |
| --- | --- | --- |
| `:acquisition-radius-pixels` | `float` | |
| `:retention-radius-pixels` | `float` | |
| `:centre-tie-radius-pixels` | `float` | |
| `:switch-improvement-ratio` | `float` | |
| `:range-transition-start-multiplier` | `float` | |
| `:minimum-radius-pixels` | `float` | |
| `:maximum-radius-pixels` | `float` | |
| `:bounds-padding-pixels` | `float` | |
| `:bracket-start-radius-multiplier` | `float` | |
| `:opacity` | `float` | |
| `:pulse-opacity-boost` | `float` | |
| `:pulse-duration` | `settings_nonnegative_float` | |
| `:fade-out-duration` | `settings_nonnegative_float` | |
