# S7 level authoring

- Save the map. Open **Space Game Level** and choose **Create Level Script** or **Link Existing Script**. Save the map again to keep its project-relative link and Play setup.
- Place a player ship, or enable **Use Observer Camera** and select its targets. Use **Show Collision Grid** and **Show Collision Bounds** to check placements before export and during Play.
- Enable **Details → Level Authoring → Export** to include an actor. Its **Script ID** comes from **Custom Name** when set, otherwise from the Outliner label. Export converts the name to a lowercase Scheme symbol.
- Set **Custom Name** to keep a fighter, capital ship, or turret ID stable across team changes. Update references in handwritten logic when an exported ID changes.
- Run **Check Scene** to validate pending placements, script logic and shared assets. Filter the **Output Log** by `LogS7LevelAuthoring` for results.
- Save the map or use **Export Scene** to write placements, overrides and viewpoint to `<level>/editor-config.scm`. Autosave skips export. If export fails, the map still saves.
- Write logic and transformations in **Edit Level Script**. **Play** uses the saved script and logs a reminder for pending scene edits.
- **Rebuild Scene from Script** reloads placements and the viewpoint from disk. Pending scene edits block rebuild and produce an Output Log warning. Use **Export Scene** to keep them.
- **Force Rebuild from Script** asks for confirmation, replaces bound actors and removes newly placed actors with **Export** enabled. Unbound actors with **Export** off stay in the map. You can undo a rebuild. Edit script-only placements in Scheme.
- Open **Level → Shared Level Assets** to change meshes, palettes, effects or collision class lists for all linked levels.
- Use **Tools → Export Entity Geometry** after changing shared meshes, sockets or fighter wave size.

See [Level scripts](../../../LevelScripts/README.md) for Scheme examples.
