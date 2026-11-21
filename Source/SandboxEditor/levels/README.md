# S7 level authoring

- Save the map. Open **Space Game Level** and choose **Create Mission** or **Link / Change Script**. Save the map again to keep its project-relative link and Play setup.
- Place a player ship, or enable **Use Observer Camera** and select its targets. Keep placements inside the collision grid.
- Export actors with **Details → Level Authoring → Export**. Name new actors in the Outliner before their first export. Use **Rename Entity ID** for bound actors.
- Run **Check Export** to test the scene against the linked mission. Save the map or use **Export Scene** to write placements, overrides and viewpoint to `<mission>/editor-config.scm`. Autosave skips export. If export fails, the map still saves.
- Write mission logic and transformations in **Edit Mission Source**. **Play** uses the saved script and logs a reminder for pending scene edits.
- Use **Rebuild Scene** to replace bound proxies with saved placements. Baseline actors enable **Export**. Script-only actors disable it. Unbound actors stay in place. Undo restores the previous scene.
- Use **Tools → Export Entity Geometry** after changing shared meshes, sockets or fighter wave size.

See [Level scripts](../../../LevelScripts/README.md) for Scheme examples.
