# SpaceGame

SpaceGame contains the runtime presentation configuration and editor/runtime collision diagnostics
for the space-combat simulation. Configure sparks through the level configuration and orchestrator
presentation settings; use the editor preview and runtime visualisation when authoring fighter spawns.

Impact sparks use GPU expansion. A procedural renderer required more CPU bookkeeping and showed
no measurable GPU or frame-time improvement in the comparison.

See [ARCHITECTURE.md](ARCHITECTURE.md) for collision contracts, static harvesting, and spawn-layout
ownership.
