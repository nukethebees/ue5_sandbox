# Author a mission

Copy [Border Skirmish](BorderSkirmish.scm), give the level a unique `:id`, and export `mission`. Add its path to [the catalog](catalog.scm) or a [campaign](Campaigns/). Use symbols for IDs and quoted lists for positions. Imports resolve from `LevelScripts/` and expose only declared exports.

For editor-backed levels, keep mission logic in `LevelName.scm`. **Export Scene** writes placements and viewpoint to `LevelName/editor-config.scm`. Apply settings in order: C++ defaults, [shared settings](Settings/base.scm), editor overrides, mission overrides.

Run a saved level headlessly:

```powershell
coj benchmark native-simulation --level LevelScripts/BorderSkirmish.scm --seconds 10
```

[Mission logic](docs/logic.md) · [Edit a map in Unreal](../Source/SandboxEditor/levels/README.md) · [Loading pipeline](../native/level_authoring/ARCHITECTURE.md)
