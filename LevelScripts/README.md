# Author a mission

Copy [Border Skirmish](BorderSkirmish.scm), give the level a unique `:id`, and export `mission`. Add its path to [the catalog](catalog.scm) or a [campaign](Campaigns/). Use symbols for IDs and quoted lists for positions. Imports resolve from `LevelScripts/` and expose only declared exports.

Keep mission logic in `LevelName.scm`. Save the linked map or use **Export Scene** to write placements and viewpoint to `LevelName/editor-config.scm`. Layer editor overrides over [level defaults](Settings/level-defaults.scm), then apply mission overrides.

Place a **Fighter Proxy** to set its position, heading, and team. Export actors with **Details → Level Authoring → Export**. Carrier launches share `:max-live-auto-spawned-fighters` across authored teams, including teams with future spawns. Placed fighters bypass the cap.

Run a saved level headlessly:

```powershell
coj benchmark native-simulation --level LevelScripts/BorderSkirmish.scm --seconds 10
```

[Mission logic](docs/logic.md) · [Edit a map in Unreal](../Source/SandboxEditor/levels/README.md) · [Loading pipeline](../native/level_authoring/ARCHITECTURE.md)

## Settings

- Set process and debug flags in [application.scm](Settings/application.scm).
- Tune gameplay and Starfox flight in [simulation.scm](Settings/simulation.scm) and visuals in [presentation.scm](Settings/presentation.scm). Use **Tools → Export Entity Geometry** after changing shared meshes, sockets, or fighter wave size.
- Set factory preferences in [user-defaults.scm](Settings/user-defaults.scm). Edit personal overrides in `Settings/user-settings.scm` or use **Options → Apply**. Packaged games store overrides under the local application settings directory in `IsleOfJebbers/Config/`.
- Reset a category in **Options**, then **Apply**. Enhanced Input saves key mappings separately.

To update an older personal file, import the defaults and move its existing fields into `copy-with`.

```scheme
(export preferences)
(define defaults (module-ref (import-module "Settings/user-defaults.scm") 'preferences))
(define preferences (copy-with defaults :mouse-turn-sensitivity 2))
```
