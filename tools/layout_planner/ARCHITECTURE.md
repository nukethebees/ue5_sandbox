# Memory layout planner architecture

The headless library lives in `native/layout/lib`. LispB supplies parsing,
semantic resolution and source rendering. The SDL/ImGui application submits
changes and reads the resulting document and analysis tables.

## Ownership and replay

`EditorSession` owns durable command/source strings, diagnostics and resettable
compiler workspaces. `Document` identifies one session-owned document. Moving a
document does not move its storage or invalidate queued transactions.

Transactions contain typed instructions in SoA columns. Names resolve in
instruction order, allowing create-then-edit and rename-then-edit operations.
Accepted history stores instructions and transaction boundaries. Undo and redo
move a cursor; replay rebuilds the model from the retained initial inputs and
the accepted instruction prefix. There are no model snapshots or checkpoints.

Instructions can leave intermediate states invalid. A new transaction resolves
and validates its final schema before acceptance. On rejection, diagnostics are
published and the previous accepted prefix is rebuilt without losing redo.
Physical experiments can produce unavailable analysis results without making
the authored schema invalid.

Persistent declaration, source and variant IDs survive replay. Compiler and
analysis row handles belong to the current rebuilt workspace. Reacquire query
views after processing changes. Workspace reset clears source text and strings
as well as syntax and semantic rows, retaining reusable buffer capacity.

## Inputs and publication

Initial source and registry include contents are retained in memory. Commands
capture their text and profile inputs. Replay does not reread files from disk.
Variants and analysis settings share the document's undo history but remain
session-local; they do not change the saved LispB format.

Save and Save As publish rendered sources independently of replay. They preserve
history, attached comments and literal contents, and check for external file
changes. Last-written content receipts support those checks. Rendering produces
temporary output rather than retaining a revision-indexed source cache.

## Analysis and UI

`AnalysisStorage` owns SoA tables for semantic encodings, physical layouts,
packed fields, SoA allocations, relationships, workloads and distributions.
Small optional groups represent values calculated and consumed together.
Independent facts, such as cache and page coverage, remain independent.

LispB schemas in `native/layout/schema` define the analysis tables. The
`layout-analysis` generation target emits their vector storage, views and
accessors into the build directory. Generated reset operations clear all
columns while retaining capacity. Numerical passes remain ordinary C++.

`Analyzer` borrows its inputs, output tables and diagnostic context. Ordered
passes resolve dependencies, calculate layouts, then evaluate relationships and
workloads. Identical requests share rows within one run. Each run clears and
rebuilds the tables; comparisons read existing rows instead of copying results.

Missing ABI facts remain unknown. Semantic widths do not imply object sizes.
Cache/page results describe modeled footprints, not measured traffic or
latency. Analysis errors are recorded centrally, never copied into result rows.
Parser errors are published with durable source coordinates before reset.

`PlannerController` drains queued UI actions and runs analysis once for the
resulting model and selection. Drawing consumes the last completed results and
queues further actions. Unchanged frames perform no analysis. Selection,
unsubmitted drafts, docking and preferences stay outside undo history.

## Validation

Parameterized tests use LispB source, public change/query APIs and expected
results. The opt-in document benchmark measures replay plus analysis latency
and memory over a bounded edit/undo/redo history. Timing is reported separately
from correctness tests.
