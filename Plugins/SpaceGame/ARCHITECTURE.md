# SpaceGame runtime architecture

`ATestBatchOrchestrator` connects the Unreal actor lifecycle to the native level. It retains
reflected configuration, player and component references, and orders startup, frame publication
and teardown. Simulation policy and presentation work belong to the owners below.

| Owner | Responsibility |
|---|---|
| `prepare_level_simulation` | Validate effective configuration and produce native initialization data from an authored definition or live proxies, including mission and telemetry metadata |
| `FLevelLoader` | Validate launch options and create authored player/camera actors using an optional visual context with a controller reference |
| `ioj::sim::LevelSim` | Own the worldless simulation and execute its fixed-tick phases |
| `FLevelPresentationHost` | Bind actor-owned components, maintain HUD and presentation, and display collision snapshots |
| `LevelTelemetryIntegration` | Consume finalized native reports, persist them and navigate to results |
| `reset_level_world` | Replace the player and remove transient world actors while retaining framework actors |

The presentation host belongs to `SpaceGamePresentation` and receives native read access and
explicit resources. It does not depend on the orchestrator or SpaceGame actor classes. Component
ownership stays in actor `UPROPERTY` fields; the host's component bundle only borrows them.
Rebind that bundle after loading serialized actor properties.
The actor validates its presentation resources before loading; the load request carries no cached
validity flags. An absent visual context selects headless loading.

## Startup and lifetime

Native construction creates the tick-zero entities but leaves initialization open. Bind the
player's commands, assign proxy entity IDs and broadcast those bindings while the proxy actors
are still live. Destroy the proxies before harvesting static geometry so their collision does
not become a second, static representation of simulated entities.

Install the harvested bounds before `LevelSim::finish_initialisation()`. That boundary publishes
lookup rows and the spatial index, enters `StableSetup`, initializes telemetry and the mission,
then enters `BetweenTicks` in the paused state. Initialize presentation and HUD afterward, then
activate controls and apply the requested start mode. Proxy turret transforms retain authored
scale; authored levels derive initial turret transforms from the initialized native state.

A running Unreal frame advances the native fixed ticks before presenting the completed frame.
Presentation observes the final tick of a catch-up frame. Player death and mission-completion
notifications follow presentation; telemetry persistence follows native finalization.

During reset or world teardown, finalize the run and release HUD/player bindings before destroying
the native level. Reset the presentation host while its components and simulation are still
alive. Persist the finalized report before clearing launch options. World reset publishes the
replacement player and mission references before finishing deferred spawning, and applies player
configuration before `BeginPlay`. Notify reset subscribers only after the replacement is ready.
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
