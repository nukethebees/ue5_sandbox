# SpaceGame

`ioj` is the game's working codename. Game-specific C++ code uses the concise `ml::ioj`
namespace, allowing types such as `ml::ioj::UPauseMenuWidget` to remain short while retaining clear
context and collision isolation.

## Spark configuration

The spark system has two configuration scopes. The orchestrator owns one persistent renderer for
the level, while each effect producer supplies the style of the bursts it emits. The `Sparks`
component shown beneath an orchestrator is runtime-owned and is not the place to configure burst
appearance.

### Laser impact bursts

Laser impact appearance is configured on the `USpaceGameLevelConfig` data asset assigned to the
orchestrator:

1. Select the orchestrator and open its `Level Config` asset, or open that asset directly in the
   Content Browser.
2. Expand `Laser Projectiles`, then `Impact Sparks`.
3. Edit the burst style and save the data asset.

`Count` is the exact number of particles emitted by each laser hit; zero disables laser-impact
sparks. `Size Min` and `Size Max` control particle thickness in world-space centimetres. These are
the settings to change when a burst needs more particles or physically larger sparks.

The remaining burst settings are:

| Setting | Effect |
|---|---|
| `Speed Min/Max` | Initial particle speed in centimetres per second. |
| `Lifetime Min/Max` | Particle lifetime in seconds. |
| `Intensity` | Multiplier applied to the laser's team colour before rendering. Increase this for brighter HDR sparks and stronger bloom. |
| `Spread Angle Degrees` | Half-angle of the emission cone. `0` follows the emission direction, `90` is a hemisphere, and `180` is isotropic. |
| `Streak Time` | Length of the velocity-aligned streak expressed as travel time; larger values make longer streaks. |

Laser spark colour is taken from the hit laser's team colour. The burst style changes its intensity
but does not replace its base colour.

### Level renderer settings

Renderer-wide settings are configured on the orchestrator under `Presentation Settings > Sparks`:

| Setting | Effect |
|---|---|
| `Capacity` | Maximum number of particle slots in the level-wide ring buffer. New particles overwrite the oldest slots when it is full. This is separate from `Laser Projectiles > N Preallocated Instances`, which sizes the laser projectile presentation pool. |
| `Maximum Draw Distance` | Distance beyond which sparks are rejected. |
| `Acceleration` | Constant world-space acceleration applied to every spark, normally gravity. |
| `Minimum Thickness Pixels` | Lower screen-space thickness clamp. Enlarged sparks have their energy reduced to compensate. |
| `Maximum Thickness Pixels` | Upper screen-space thickness clamp used to bound close-range fill cost. |
| `Maximum Length Pixels` | Upper screen-space streak-length clamp. |

The renderer settings are copied into the persistent component when level presentation is created.
Stop and restart Play In Editor after changing them. Burst styles are likewise intended to be
authored before starting the level; restart the level after editing the level-config asset when
checking a change.

## Collision contracts

`CollisionUniformGrid` uses a half-open world-space domain: the negative grid bounds are included
and the positive grid bounds are excluded. A trace parallel to a positive outer boundary is outside
the grid, while a trace that crosses from that boundary into the grid is valid. Traces are clipped
to the grid before cell traversal, so endpoints may otherwise lie outside it.

Entity AABBs and line segments are closed during the narrow-phase intersection test. Contact with
an AABB face, edge, endpoint, or degenerate AABB therefore counts as a hit. Broad-phase insertion is
conservative: an AABB on an internal cell boundary may be present in both adjacent cells.

Zero-length traces test their single point. The nearest hit along each segment is returned. Dynamic
and harvested static AABBs participate in the same closest-hit calculation; dynamic geometry wins
an exact-distance tie. Static hits have an invalid entity handle and identify their source through
`static_geometry_indices`. On a miss, only the hit flag is meaningful; the corresponding location
and identity values are unspecified. Inputs must be finite, and all AABBs must fit within the
configured grid. Queries may supply one ignored dynamic entity handle per trace; static geometry is
still tested normally.

Static collision is harvested once during level initialization from query-enabled simple aggregate
geometry on configured actor classes. Each supported primitive component becomes one combined
world-space AABB, and its Unreal collision is disabled only after the static grid is built. Harvested
components must remain static afterward; moving one requires rebuilding the harvested geometry.
