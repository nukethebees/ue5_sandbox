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

## Collision and fighter-spawn visualisation

### Before Play: place capital fighter spawn arrows

1. Select a capital ship proxy in the level editor.
2. Enable viewport **Realtime** so the preview refreshes while moving the capital or its arrows.
3. In the capital's Details panel, leave **Ship > Spawn Preview > Show Fighter Spawn Preview**
   enabled. It is enabled by default and only draws for selected capitals in the level editor.
4. Move the spawn arrows until their preview markers are cyan and outside the amber box.
5. Click **Save Configuration to Asset** on the capital to update the shared runtime layout.
6. Click **Apply Asset Configuration to All Instances** to synchronize the other capitals,
   then save the level-config asset and the level. Apply discards unsaved arrow edits on those
   capitals. Check clearance at each capital's rotation after applying.

| Editor preview colour | Meaning |
|---|---|
| Green box | Capital's world-space collision AABB, calculated using the runtime bounds rules. |
| Amber/orange box | Capital AABB expanded by the fighter mesh bounding-sphere radius plus `Avoidance Clearance Buffer`. The arrow's position must be outside this box; contact with its boundary is invalid. |
| Cyan sphere and directional-arrow marker | Live arrow clears its parent capital and the other spawn arrows on that capital. |
| Red sphere and directional-arrow marker | Live arrow intersects the expanded capital box, or is closer than twice the fighter clearance to another arrow on the same capital. |

The coloured markers are temporary overlays. They do not change the authored arrow colours,
positions or asset data. The preview does not run during Play.

**Save** captures the live arrows in the actor-space frame used by runtime spawning, including
the effect of their component attachments. **Apply Asset Configuration** does the reverse: it
replaces the arrows from the saved asset. Do not use Apply to preserve unsaved arrow edits.
The spawn-slot configuration is shared by capitals using that level-config asset, so check the
other capital orientations after saving it.

Under **Ship > Configuration**, **Level Config Asset** shows the resolved shared asset read-only.
**Spawn Configuration Status** reports whether live arrows match its runtime layout; matching
does not mean the layout has sufficient clearance. These fields refresh for selected capitals
with viewport Realtime enabled, even with drawing disabled.

Save, Apply, Apply All and the preview require exactly one orchestrator with a level configuration
in the editor world. They do not use a stale per-proxy asset binding. Apply All resolves that
asset once for every capital; applying is undoable as one operation and marks the level modified.
Save is also undoable but intentionally does not replace other capitals' live arrows.
Missing configuration, meshes or arrow references are reported in the Output Log.

Turret and tube-spinner proxies also resolve their Apply/Apply All configuration from the
editor world's orchestrator. These operations support undo and mark the level modified.
They retain their existing editing actions; only capitals have Save Configuration to Asset.
Automated proxy regression coverage lives in `Source/SandboxTests/unit/test_proxy_configuration.cpp`.

### During Play: inspect actual grid collision

Select the **TestBatchOrchestrator** and enable **Show Collision Bounds** under
**Sandbox > Collision > Visualization** (or search the Details panel for the setting), then Play.
Increase **Collision Bounds Max Draw Distance** if necessary; its default is 200,000 cm (2 km).

| Runtime display colour | Meaning |
|---|---|
| Green boxes | Registered entities' cached world-space collision AABBs. |
| Orange boxes | Harvested static geometry's collision AABBs—not fighter-clearance boundaries. |

Runtime visualisation reads the same cached boxes used by the grid's line and sweep queries.
Fighter navigation additionally expands solid obstacles by fighter clearance, so a fighter can
stop before reaching the displayed collision box.

### Bounds, diagnostics and current limits

An AABB remains world-axis aligned. Rotating an entity rotates its local collision-centre offset
and expands the world box to enclose the rotated local collision box. A diagonal or irregular
capital can therefore have substantial empty space inside its AABB. This is conservative collision,
not a tight hull outline; place arrows outside the clearance box even if they look far from the mesh.
The calculation uses the simulation actor's position and rotation, not the editor mesh component's
relative transform or scale.

The editor preview checks live arrows against their own capital and sibling arrows only. Cyan is
not a guarantee of clearance from other capitals, stations, world bounds or future traffic, and it
does not mean those live positions have been saved. Level-start validation also checks saved slots
against the parent capital's world bounds at its authored rotation; invalid slots prevent simulation
startup and log errors.

For saved-versus-live spawn mismatches, use **Diagnose Fighter Spawn Points** on the capital.
That separate diagnostic draws green predicted runtime positions, cyan live positions and red
connecting lines for 30 seconds. Its green/cyan colours describe position sources, not validity.
For runtime spawn/stop reports, use `sg.FighterDiagnostics 1` in the console. Reports are bounded;
toggle it off and back on while the simulation runs to rearm reporting.

The pre-Play preview currently supports capital proxies only. A future general preview should share
the runtime extraction rules for registered entity types and configured harvested static geometry,
but collect them without mutating components or starting simulation. Visible meshes that are omitted
from custom collision, or have unsupported collision geometry, must not be presented as valid grid
obstacles. Keeping that collection separate from rendering would allow one level-wide preview,
with selected-object filtering and refreshes when transforms or collision configuration change.
