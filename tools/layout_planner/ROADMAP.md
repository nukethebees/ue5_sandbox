# Memory layout planner roadmap

## Product direction

The planner is an authoring tool, not only a schema viewer. LispB S-expressions remain the
canonical source representation. The application edits a source-aware schema draft, resolves that
draft into the shared semantic type graph, and uses the graph for layout analysis and C++ code
generation.

```text
LispB source
    <-> parse / source-preserving serialization
Editable schema document
    -> resolve and validate
Semantic type graph
    -> layout analysis
    -> GUI visualization
    -> C++ code generation
```

The editable document belongs to the LispB/schema library. The GUI must not introduce planner-only
copies of enums, packed fields, columns, or future type declarations.

## Current milestone

The editable document/command foundation supports enum and packed-value authoring. Declarations can
be added to existing matching modules; ordered values/fields can be added, duplicated, deleted,
reordered, and edited inline; packed widths can also be changed through the bit diagram. Semantic
edits support undo/redo, affected-source preview, and validated save/reload. Module creation, type
rename/deletion, and source-token-preserving edits within touched declarations remain follow-up work.

Project Open, Save, validated Save As cloning, persisted recent-project history, and module-first
schema browsing are implemented. A native file picker and richer multi-target project selection are
later usability work; the current path dialogs deliberately keep the file workflow simple.

Packed analysis also supports selectable element-count presets and custom counts, reporting
overflow-safe aggregate bytes, unused bits, minimum cache lines, and minimum pages. Cache-line and
page sizes now come from an explicit x86/x86-64 baseline profile with provenance; unknown profile
facts remain unknown rather than falling back to analyzer literals.
Standard-library SoA analysis also reports per-column and aggregate minimum pages using the target
page size, treating each vector as a separate allocation and excluding unknown allocator overhead.

Enum inspection now derives literal and implicit value domains, count-sentinel code use, minimum
semantic bit width, target-backed signed/unsigned fit, and unused backing codes. General initializer
expressions remain explicitly unknown. Durable explicit-width/value-domain enum authoring remains a
future shared-schema change rather than planner-only metadata.

The shared LispB schema now has an initial ordinary `record-module`: records contain semantic
members and optional fixed element counts, resolve to `RecordType` nodes and dependency edges, and
lower to dependency-ordered ordinary C++ structs with `std::array` for fixed arrays. Illegal
by-value record cycles are rejected. Target-derived record analysis reports recursive member
offsets, fixed-array extents, alignment, internal/tail padding, and total size in a byte map while
keeping unknown facts explicit. Record declarations are now source-backed and editable through a
flat member grid with add/duplicate/delete/reorder, scalar/fixed-array cardinality, preview,
undo/redo, and save/reload. Cache/page scaling and richer member type selection remain follow-up.

## 1. Editable document and command foundation

- Own a mutable draft of the validated LispB declarations.
- Retain source-file ownership and source ranges for declarations.
- Assign stable declaration IDs that survive ordinary edits.
- Apply typed semantic commands rather than text substitutions.
- Re-resolve and validate the semantic graph after each command.
- Keep invalid operations from corrupting the last valid draft.
- Provide undo/redo, dirty state, and a monotonic revision for consumers.
- Track the saved history position independently from the edit revision.
- Later extend this foundation with precise serialization, atomic save, diff preview, and
  reparse-after-save verification.

## 2. Enum authoring vertical slice

- Create an enum in a selected module and namespace.
- Select its underlying semantic type.
- Add, remove, duplicate, and reorder enumerators.
- Edit symbolic names, explicit values, display names, serialized names, hidden markers, and count
  sentinel semantics already supported by LispB.
- Rename and delete safely with dependency-aware diagnostics.
- Save the declaration to LispB, reload it, resolve the same semantic graph, and generate the
  expected C++.

## 3. Packed-value and bit-field authoring

The initial end-to-end slice is implemented; reserved-bit and richer sentinel semantics remain
future schema work.

- Create packed values with a backing storage type and optional invalid raw value.
- Add, remove, duplicate, and reorder fields.
- Select each field's logical semantic type.
- Edit bit width, packed field kind, and existing range-helper metadata.
- Support both numeric edits and direct divider dragging.
- Display unused bits, overflow, representable ranges, and dependency edges immediately.
- Allow creation of a referenced enum from the field workflow.
- Save and reload the resulting LispB without losing semantics.

The primary acceptance case is creating an enum backed by `uint8`, then creating a packed `uint32`
value with a 24-bit integer field and an 8-bit field that semantically references that enum.

## 4. SoA authoring

The initial standard-library SoA vertical slice is implemented. Rich fixed/nested/mask metadata is
preserved by whole-declaration editing; direct UI for those advanced properties remains follow-up.

- Create standard-library SoA declarations.
- Add, remove, duplicate, and reorder columns.
- Select semantic column types and supported column kinds.
- Navigate to or create referenced types.
- Keep planning-only capacity experiments separate from source semantics unless capacity becomes a
  declared LispB property.
- Save, reload, analyze, and generate the declaration.

## 5. Relationship and declaration lifecycle operations

An initial dockable pan/zoom relationship graph is implemented over the resolved semantic graph,
with selectable nodes and field-aware dependency labels. Relationship editing and richer semantic
relationship kinds remain future work.

- Provide a searchable semantic type picker.
- Navigate from a field or column to its referenced definition.
- Rename declarations while updating semantic references deliberately.
- Report all reverse users before deletion.
- Reject unsafe deletion or require explicit reference repair.
- Duplicate declarations under a new stable identity.
- Move declarations between modules where legal.
- Keep unresolved references as explicit draft errors; never silently convert them to external
  leaves.

## 6. Semantic design variants

Evolve variants from physical override maps into typed change sets over the baseline document.
Variants may add, remove, rename, or structurally edit declarations and relationships. Comparison
should report both semantic changes and their physical consequences. Recovery state may preserve
draft work, but accepted durable changes are written to LispB rather than a competing schema
format.

## 7. Struct and AoS synthesis

Extend the shared type graph and authoring document with structures, typed members, fixed arrays,
alignment, offsets, internal padding, and tail padding. Support member-order experiments and
hot/cold grouping using the same command, validation, serialization, and dependency mechanisms.

## 8. Workload and system-level planning

- Compose semantic types into explicit planning scenarios.
- Model counts, capacities, multiplicity, allocation strategies, and selected access sets.
- Report payload, padding, capacity slack, allocation count, cache lines, pages, and total memory.
- Compare complete subsystem costs and identify the largest contributors.
- Reference semantic types by stable identity rather than redeclaring their schemas.

## 9. Physical-fact accuracy

- Record provenance for every ABI fact.
- Load generated `sizeof` and `alignof` facts from the real target compiler and configuration.
- Identify profiles by platform, architecture, compiler, and build configuration.
- Continue reporting unknown facts instead of guessing.
- Model allocator and single-allocation details only when the selected backend supplies factual
  rules.

## 10. Advanced layouts

After the authoring and analysis loop is reliable, extend the semantic graph with unions, tagged
unions, arrays, AoSoA/chunking, handles, references, containers, and arena/block allocation.
Performance claims should be based on explicit access patterns or measurements rather than a
speculative aggregate score.

## Validation strategy

Keep authoring and analysis behavior testable without ImGui. Cover command inversion, undo/redo,
stable identity, duplicate and unresolved references, source ownership, serialization round trips,
ABI boundary cases, overflow-safe aggregates, and unchanged generated C++ for unedited schemas.
Each new declaration kind should have an end-to-end create, save, reload, resolve, analyze, and
generate acceptance case.
