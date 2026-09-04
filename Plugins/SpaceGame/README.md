# SpaceGame

`ioj` is the game's working codename. Game-specific C++ code uses the concise `ml::ioj`
namespace, allowing types such as `ml::ioj::UPauseMenuWidget` to remain short while retaining clear
context and collision isolation.

## Collision contracts

`CollisionUniformGrid` uses a half-open world-space domain: the negative grid bounds are included
and the positive grid bounds are excluded. A trace parallel to a positive outer boundary is outside
the grid, while a trace that crosses from that boundary into the grid is valid. Traces are clipped
to the grid before cell traversal, so endpoints may otherwise lie outside it.

Entity AABBs and line segments are closed during the narrow-phase intersection test. Contact with
an AABB face, edge, endpoint, or degenerate AABB therefore counts as a hit. Broad-phase insertion is
conservative: an AABB on an internal cell boundary may be present in both adjacent cells.

Zero-length traces test their single point. The nearest hit along each segment is returned. When two
hits have the same distance, callers must not depend on which entity is selected. On a miss, only the
hit flag is meaningful; the corresponding location and entity values are unspecified. Inputs must
be finite, and entity AABBs must fit within the configured grid.
