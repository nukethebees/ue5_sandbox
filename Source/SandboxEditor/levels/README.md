# S7 level authoring

- Save the map. Open **Space Game Level** and choose **Create Level Script** or **Link Existing Script**. Save the map again to keep its project-relative link and Play setup.
- Place a player ship, or enable **Use Observer Camera** and select its targets. Use **Show Collision Grid** and **Show Collision Bounds** to check placements before export and during Play.
- Export actors with **Details → Level Authoring → Export**. Set **Custom Name** on a fighter, capital ship, or static turret to override its generated label. To rename a bound actor’s **Script ID**, select it, edit the field and press Enter. Update handwritten script references.
- Generated proxy labels show the current team in the Outliner and authoring viewport. Custom labels such as `escort` and bound script IDs survive team changes.
- Run **Check Scene** to validate pending placements, script logic and shared assets. Filter the **Output Log** by `LogS7LevelAuthoring` for results.
- Save the map or use **Export Scene** to write placements, overrides and viewpoint to `<level>/editor-config.scm`. Autosave skips export. If export fails, the map still saves.
- Write logic and transformations in **Edit Level Script**. **Play** uses the saved script and logs a reminder for pending scene edits.
- **Rebuild Scene from Script** reloads placements and the viewpoint from disk. Pending scene edits block rebuild and produce an Output Log warning. Use **Export Scene** to keep them.
- **Force Rebuild from Script** asks for confirmation, replaces bound actors and removes newly placed actors with **Export** enabled. Unbound actors with **Export** off stay in the map. You can undo a rebuild. Edit script-only placements in Scheme.
- Open **Level → Shared Level Assets** to change meshes, palettes, effects or collision class lists for all linked levels.
- Use **Tools → Export Entity Geometry** after changing shared meshes, sockets or fighter wave size.

See [Level scripts](../../../LevelScripts/README.md) for Scheme examples.
