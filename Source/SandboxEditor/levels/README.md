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
3. Click **Adopt Entities** after placing actors. Leave **Export** enabled for entities whose
   placement is owned by the editor, then click **Export Scene**.
4. The new entry point layers editor settings over `Settings/base.scm`. Add mission overrides
   last in `settings-layers` and put mission logic in the entry point. Use **Save Source**
   before validation or Play.
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
and use `level-ref` / `level-with` to add entities, objectives and settings with ordinary Scheme.
**Save** writes the human source buffer exactly; export never rewrites it. Use **Load Source** to
associate another existing root. There is no Save As operation that relocates a level's files.

**Import Source** loads generated entities from the generated baseline and human-added entities
from the composed root. Generated entities default to **Export** enabled; human-added entities
default to disabled. Explicit ownership choices survive refresh by authored entity ID within
the same level. Switching roots resets those defaults. Human transformations of generated entities
apply during gameplay without being copied back into editable baseline values.

Edits to a proxy with Export disabled are not saved back to human source. Enabling Export for an
entity still declared by the human source produces a normal duplicate-ID validation error.

PIE exports editor data, evaluates the saved human entry point, validates the complete level and
compiles simulation initialization before authorizing play. Missing or invalid roots reject PIE.
The prepared definition launches directly; preview actors do not initialize gameplay entities.
Save source-buffer changes before PIE. Native and packaged loading use the same Scheme entry point
without editor actors.

Shared meshes, classes and other Unreal resources remain in the level configuration asset.
Gameplay settings belong in Scheme. Full serialization preserves resolved values; difference
serialization cannot distinguish a value explicitly pinned to its baseline from an inherited value.
