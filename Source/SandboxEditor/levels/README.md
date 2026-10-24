# S7 level authoring

Edit level identity, title, description, objectives and settings in the S7 source editor.
Save writes the source buffer exactly. Spatial placement, entity bindings, camera targets and
collision-grid dimensions retain their visual controls.

The document's hidden metadata properties mirror the applied script for scene comparisons,
serialization and Undo. They are not a second editable source of level metadata.

Canonical scene export remains limited to the supported scene subset. Mission definitions,
settings layers, unlocks, par time and entity overrides must be maintained in the source;
the scene exporter rejects these features rather than silently discarding them.
