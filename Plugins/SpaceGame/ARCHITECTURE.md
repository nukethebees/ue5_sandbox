# SpaceGame runtime architecture

`ATestBatchOrchestrator` connects the Unreal actor lifecycle to the native level. It retains
reflected configuration, player and component references, and orders startup, frame publication
and teardown. Simulation policy and presentation work belong to the owners below.

| Owner | Responsibility |
|---|---|
| `prepare_level_simulation` | Validate effective configuration and compile an authored definition with Unreal mesh and collision resources into native initialization data |
| `configure_level_run` | Apply run metadata and optional duration-limit events to initialization data |
| `FLevelLoader` | Validate launch options and create authored player/camera actors using an optional visual context with a controller reference |
| `ioj::sim::LevelSim` | Own the worldless simulation and execute its fixed-tick phases |
| `FLevelPresentationHost` | Bind actor-owned components, maintain HUD and presentation, and display collision snapshots |
| `LevelTelemetryIntegration` | Consume finalized native reports, persist them and navigate to results |
| `reset_level_world` | Remove gameplay actors while retaining framework actors; the level loader constructs the next authored level |

The presentation host belongs to `SpaceGamePresentation` and receives native read access and
explicit resources. It does not depend on the orchestrator or SpaceGame actor classes. Component
ownership stays in actor `UPROPERTY` fields; the host's component bundle only borrows them.
Rebind that bundle after loading serialized actor properties.
The actor validates its presentation resources before loading; the load request carries no cached
validity flags. An absent visual context selects headless loading.

## Startup and lifetime

Every playable level uses a Scheme entry point or a definition already decoded from that entry
point. Editor export writes the generated configuration included by the human-owned root. PIE
validates and compiles that composed definition before launch and hands its initialization data
to the orchestrator. Packaged and native launches use the same definition and native compiler.
Editor proxies are authoring objects and never initialize simulation entities.

Harvest static geometry after the editor proxies have been removed from the PIE world, and put
the bounds in `LevelSimInitData::collision.static_bounds` before construction. Native construction
dispatches initial events, publishes entity lookup rows and the spatial index, initializes telemetry
and resets its clock to tick zero and `BetweenTicks`. The simulation returns paused, or stopped if
an initial terminal event ends the run. Bind player commands and initialize presentation and HUD
afterward, then activate controls and apply the requested start
mode. Presentation derives initial entity transforms from the initialized native state.
`LaunchOptions::presentation_mode` controls presentation throughout the run.

Use `get_level_simulation()` for native timing, entity simulations, queries, logic results and telemetry.
It returns null before construction and after release. Actor accessors cover configuration,
presentation, player actors and launch behavior; `get_state()` also covers the absence of a level.

A running Unreal frame advances the native fixed ticks before presenting the completed frame.
Presentation observes the final tick of a catch-up frame. Player death and level-completion
notifications follow presentation; telemetry persistence follows native finalization.

The actor keeps ticking while the level is paused. Every ready-level frame calls `advance()` to
consume between-tick requests, while paused calls leave simulation time and presentation unchanged.
Pause requests take effect on the next advance; the orchestrator then notifies the controller to
open a pending pause menu and refresh run controls. Resume remains direct. Paused launches enter
that state during initialization without queueing a pause request.

During reset or world teardown, finalize the run and release HUD/player bindings before destroying
the native level. Reset the presentation host while its components and simulation are still
alive. Persist the finalized report before clearing launch options. World reset removes gameplay
actors and clears prepared data. The next explicit authored load constructs its player and camera
through the usual loader. Reset subscribers observe the cleared world.
Automation leaves finalized telemetry available for explicit test inspection.

## Collision and spawning

Simulation collision uses conservative world-axis-aligned boxes in a bounded grid. Broad-phase
insertion may duplicate internal-boundary boxes; narrow-phase treats AABBs and line segments as
closed, supports zero-length traces, and returns the nearest hit. Dynamic entities win exact-distance
ties over harvested static geometry. Inputs must be finite and fit the configured grid.

Configured query-enabled simple static geometry is harvested once at level initialization into the
same closest-hit path as dynamic collision. Harvested components must remain static; moving one
requires rebuilding the data. The editor preview is separate presentation tooling and is not proof
of level-wide runtime clearance.

Capital fighter spawn slots are shared level-configuration data. Save captures live actor-space
arrows; Apply reconstructs them. Runtime validation checks saved slots against parent-capital bounds,
while diagnostics compare authored positions with predicted runtime positions without changing
simulation state.

See [README.md](README.md) for authoring entry points.
