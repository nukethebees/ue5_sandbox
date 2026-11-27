# Author a level

Copy [Border Skirmish](BorderSkirmish/BorderSkirmish.scm), give the level a unique `:id`, and export `level-definition`. Add its path to [the catalog](catalog.scm) or a [campaign](Campaigns/). Use symbols for IDs and quoted lists for positions. Imports resolve from `LevelScripts/` and expose only declared exports.

Keep level logic in `LevelName/LevelName.scm` beside its `editor-config.scm`. Save the linked map or use **Export Scene** to write placements, viewpoint and static collision to that file. Script-only levels can supply the same boxes, for example `:static-collision (list (world-aabb :min '(0 0 0) :max '(100 200 80)))`. Both corners use world coordinates.

Use `(merge-settings game-settings scene-settings level-settings)` to combine [level defaults](Settings/level-defaults.scm), editor settings, and level overrides. Settings merge field by field, including nested fields. When two arguments set the same field, the later value wins.

Place a **Fighter Proxy** to set its position, heading, and team. Export actors with **Details → Level Authoring → Export**. Carrier launches share `:max-live-auto-spawned-fighters` across authored teams, including teams with future spawns. Placed fighters bypass the cap.

Run a saved level headlessly:

```powershell
coj benchmark native-simulation --level LevelScripts/BorderSkirmish/BorderSkirmish.scm --seconds 10
```

[Level logic](docs/logic.md) · [Edit a map in Unreal](../Source/SandboxEditor/levels/README.md) · [Loading pipeline](../native/level_authoring/ARCHITECTURE.md)

## Settings

Browse the [Scheme settings reference](Settings/REFERENCE.md) for field types and enum values.

- Set process and debug flags in [application.scm](Settings/application.scm).
- Tune gameplay and Starfox flight in [simulation.scm](Settings/simulation.scm) and visuals in [presentation.scm](Settings/presentation.scm). Use **Tools → Export Entity Geometry** after changing shared meshes, sockets, or fighter wave size.
- Set factory preferences in [user-defaults.scm](Settings/user-defaults.scm). Edit personal overrides in `Settings/preferences.scm` or use **Options → Apply**. Packaged games store overrides under the local application settings directory in `IsleOfJebbers/Config/`.
- Reset a category in **Options**, then **Apply**. Controls saves key overrides in the same preferences file.

To override one preference, copy its category from the defaults.

```scheme
(export preferences)
(define defaults (module-ref (import-module "Settings/user-defaults.scm") 'preferences))
(define preferences
  (copy-with defaults
    :controls (copy-with (user-settings-ref defaults :controls) :mouse-turn-sensitivity 2)))
```
