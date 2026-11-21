# S7 level authoring

- Save the map. Open **Space Game Level** and choose **Create Level Script** or **Link Existing Script**. Save the map again to keep its project-relative link and Play setup.
- Place a player ship, or enable **Use Observer Camera** and select its targets. Keep placements inside the collision grid.
- Export actors with **Details → Level Authoring → Export**. Name new actors in the Outliner. To rename a bound actor’s **Script ID**, select it, edit the field and press Enter. Update handwritten script references.
- Run **Check Scene** to validate pending placements, script logic and shared assets. Save the map or use **Export Scene** to write placements, overrides and viewpoint to `<level>/editor-config.scm`. Autosave skips export. If export fails, the map still saves.
- Write logic and transformations in **Edit Level Script**. **Play** uses the saved script and logs a reminder for pending scene edits.
- Move an exported actor and save the map to update its placement. Use **Rebuild Scene from Script** to replace bound actors from disk. Unexported moves are lost. Edit script-only placements in Scheme.
- Open **Level → Shared Level Assets** to change meshes, palettes, effects or collision class lists for all linked levels.
- Use **Tools → Export Entity Geometry** after changing shared meshes, sockets or fighter wave size.

See [Level scripts](../../../LevelScripts/README.md) for Scheme examples.
