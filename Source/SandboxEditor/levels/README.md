# S7 level authoring

Every playable authored map has a human-owned `LevelName.scm` entry point and an editor-owned
`LevelName/editor-config.scm`. Configuring a new map creates a root that loads the generated file;
an existing root is never replaced. The generated baseline need not be playable by itself.

## Create a playable entry point

1. Save the map with its intended name, then click **Space Game Level** in the editor toolbar.
   Entering this mode creates an S7 authoring document if the map has none, adopts supported
   existing actors, and creates `LevelScripts/<MapName>.scm` plus
   `LevelScripts/<MapName>/editor-config.scm`. Existing files are preserved.
2. Place a player ship (`TestSpaceShip`, or the project's configured player-ship Blueprint).
   Alternatively, enable **Use Observer Camera** on the authoring document and choose adopted
   entities as camera **Targets**, with a nonzero **Offset Direction** and positive **Distance**.
   Keep the entities inside the level's collision grid.
3. Mark actors for export under **Details → Level Authoring → Export**. Click **Export Scene**
   to save placements and adopt new supported actors.
4. The new entry point layers editor settings over `Settings/base.scm`. Add mission overrides
   last in `settings-layers` and put mission logic in the entry point. Use **Edit Mission Source** to open the file in your external editor and save it there.
5. Click **Validate Level** and resolve any diagnostics shown in the mode's status area.
   Click **Set Up Playable Level** to configure the game mode, orchestrator and presentation
   actors, then save the map and press **Play**.

To use an existing level instead, open **Space Game Level** and choose **Load & Apply S7**,
select its human-owned `.scm` entry point, then validate, set up play and save the map.
If an already-associated entry point is missing, restore it from version control or load
another existing root; **New** does not replace an existing authoring document or recreate
its missing source. See [Level scripts](../../../LevelScripts/README.md) for Scheme examples.

## Source and scene ownership

Use **Export Scene** to write entity placements, supported spawn overrides and viewpoint data to
the generated file. Unchanged output is not rewritten. The main source can include this baseline
and use `level-ref` / `copy-with` to add entities, objectives and settings with ordinary Scheme.
**Load & Apply S7** associates a different saved mission with the map.

**Import Source** loads generated entities from the generated baseline and human-added entities
from the composed root. Generated entities default to **Export** enabled; human-added entities
default to disabled. Explicit ownership choices survive refresh by authored entity ID within
the same level. Switching roots resets those defaults. Human transformations of generated entities
apply during gameplay without being copied back into editable baseline values.

Edits to a proxy with Export disabled are not saved back to human source. Enabling Export for an
entity still declared by the human source produces a normal duplicate-ID validation error.

**Validate Level** compiles the saved entry point. **Play** loads that same entry point and logs
a warning if the scene needs exporting. Use **Export Scene** to play changed placements,
spawn overrides or viewpoint. Shared meshes and classes live in the level configuration asset.
Editor settings start empty and preserve explicitly set fields across exports.

Use **Export Entity Geometry** after changing a shared ship mesh, socket or launch layout.
Play and native runs read the exported values from `Settings/entity-geometry.scm`.
