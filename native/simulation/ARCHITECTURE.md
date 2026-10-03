# Simulation architecture

The simulation uses explicit, compile-time systems and generated SoA containers. Entity identity,
active storage layout, and component lifetime are separate concerns.

See [phases and data validity](docs/phases-and-data-validity.md) for the publication and mutation
contract.

## Entity lookup

`EntityUniqueId` is the persistent external identity: entity type and type-local lifetime offset.
Death does not erase its history or make the identity available for reuse. Each entity type owns
a lookup table from that offset to a generated 32-bit `EntityInstanceHandle`:

- 18 bits for the active row;
- 3 bits for team;
- 2 bits for quantised health;
- 9 reserved bits, initially zero.

The 18-bit limit applies to simultaneous active rows, not lifetime identities. Preserve each
type's existing lifetime allocation budget independently of its active-row limit.

Resolve batches grouped by entity type. Bind each type's table and component spans once per run.
Keep temporary outputs caller-owned, with the caller choosing the scratch resource. Do not retain
active handles across structural mutation.

Health uses fixed type ranges and the active row directly. Its range base is known at compile time.
Each type rebuilds every active handle from the current row, team, and health during Preparation.
Health writes directly update the fixed ranges; no dirty flags or change queues are maintained.
Packed health is a Thinking snapshot. Existing consumers continue to use authoritative component
data until explicitly migrated.

## Roadmap, not scope of this refactor

The intended ECS/component model is compile-time and code-generated, not a runtime ECS. Different
component storage policies are intentional:

- Hot movement and rotation components may later use globally dense, contiguous SoA storage for
  one-pass batched/SIMD processing across entity types.
- Cold mandatory components can use direct fixed ranges based on entity capacities.
- Components requiring independent dense lifetime or ordering may use generated component-index
  maps plus a reverse stable `EntityUniqueId` owner to repair mappings after swap-remove.
- Small qualitative/query data can live in per-instance handles, refreshed at phase boundaries
  to avoid unnecessary component reads during Thinking.

Generated mappings must support batch resolution. Do not introduce scalar lookup facades as the
default processing model. These future storage migrations are separate work; the current change
does not move movement, rotation, or AI onto new component storage.
