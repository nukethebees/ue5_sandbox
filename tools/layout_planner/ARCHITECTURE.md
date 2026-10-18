# Memory layout planner architecture

The headless API lives in `native/layout/lib`, with GoogleTest coverage in
`native/layout/tests`. LispB owns parsing, compiler storage, semantic resolution,
and canonical source rendering. The SDL/ImGui application consumes the document
and analysis APIs; it does not mutate compiler columns.

## Document and transactions

`Document` owns one compiler storage, the current module roots, project sources,
physical variants, diagnostics, and one history cursor. `Transaction` queues
instructions in parallel columns. Names resolve in instruction order, so later
instructions can use a declaration created or renamed earlier in the transaction.

Applying an instruction appends immutable declaration/module versions to the
compiler's SoA stores. Intermediate versions may be invalid. Commit resolves and
validates the final schema before publishing its roots. Rejection retains the
previous document, cursor, and redo branch; diagnostics remain available through
`diagnostics()` and `last_diagnostic_begin()`.

Accepted history consists of forward module-root splices and changed source and
variant versions. Undo and redo move the cursor, replay the accepted prefix from
the loaded baseline, then resolve the graph once. They do not reparse files or
append declaration versions. Saving leaves the baseline and history intact.

Declaration IDs survive typed edits, rename, move, and replay. Semantic `TypeId`s
belong to the resolved graph revision. Variants use durable declaration IDs and
member names; named member edits update their overrides. Variants are session
experiments and are not written into LispB source files.

## Reading and writing

Tests and consumers use source text and named queries:

```cpp
auto document = Document::from_source(source);
auto transaction = document.transaction();
transaction.set_field_bits("game", "ShipState", "health", 12);
document.commit(transaction);
auto bits = document.query().module("game").packed_value("ShipState")
                    .field("health").bits();
```

Missing query results propagate as empty optionals. Query objects borrow document
storage. Raw declaration and whole-source replacement support all LispB forms;
named methods provide convenient structural and field edits.

Canonical rendering writes changed files as complete files. Comment provenance
follows their owning modules, declarations, and children. Literal payloads and
file line endings are retained; incidental whitespace is not a persistence
contract. Save checks external changes and stages files before publishing them.
Project source membership participates in the same history as schema edits.

## Analysis and presentation

`Analyzer` combines the resolved graph, explicit ABI facts, workloads, and variant
overrides. `LayoutWorkspace` supplies the resolved analysis inputs; it has no
authored document or undo history. `PlannerSession` recomputes results when the
document revision or analysis inputs change. Analysis errors also enter the
document diagnostic container.

Physical sizes remain unknown when target facts are absent. Semantic bit widths
do not establish C++ object size/alignment. Cache/page results describe modeled
footprints rather than measured traffic. These rules are shared by tests and UI.

The UI owns selection, text drafts, docking, and preferences. Its declaration and
source editors submit transactions; dependency lists and analysis tables consume
the resolved results. C++ generation remains available through the LispB CLI.

## Validation

Parameterized tests use inline LispB sources, transaction functions, and public
verification callbacks. The opt-in `native-layout-document-benchmark` runs 1,000
mixed transactions against a project and reports commit/undo/redo plus selected
analysis latency and process memory. It is not a timing assertion in CTest.
