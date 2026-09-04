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
