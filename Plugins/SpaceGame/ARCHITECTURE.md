# SpaceGame collision and spawn architecture

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
