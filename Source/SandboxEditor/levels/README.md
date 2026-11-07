# S7 level authoring

Every playable authored map has a human-owned `LevelName.scm` entry point and an editor-owned
`LevelName/editor-config.scm`. Configuring a new map creates a root that loads the generated file;
an existing root is never replaced. The generated baseline need not be playable by itself.

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
