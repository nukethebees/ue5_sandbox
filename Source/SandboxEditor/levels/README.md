# S7 level authoring

Edit level identity, title, description, objectives and settings in the S7 source editor.
Save writes the source buffer exactly. Spatial placement, entity bindings and camera targets
retain their visual controls. Collision-grid values belong to the ordinary settings section.

The document's hidden metadata properties mirror the applied script for scene comparisons,
serialization and Undo. They are not a second editable source of level metadata.

Preview, apply and play replay the saved system settings and level source. Unsaved changes
offer save, discard or cancel; validation completes before the scene changes. The document
retains the applied, evaluated source inputs for Undo; editing an imported file takes effect
on the next replay.

Canonical scene export preserves the supplied settings fields as their last applied values.
Mission definitions, unlocks, par time and entity overrides must be maintained in the source;
the scene exporter rejects these features rather than silently discarding them.
