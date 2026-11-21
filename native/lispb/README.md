# Generate code and assets

Edit the inputs named in the [project manifest](../../lispb/project.lispb). From a prepared worktree, regenerate committed C++ and check for stale output:

```powershell
cmake --build --preset native --target generate-code
cmake --build --preset native --target check-generated-code
```

```mermaid
flowchart LR
    S[Schema] --> G[Resolved type graph]
    G --> P[Layout Planner]
    G --> I[Typed C++ IR]
    K[Kernel operations] --> I
    W[Slate widget trees] --> I
    I --> C[C++ source]
    M[Material graphs] --> U[Unreal material assets]
```

| Input | What to edit |
| --- | --- |
| Schema | Define a logical type. Add `(projections (cpp))` for an owning C++ type. [Entity IDs](../../lispb/schema/native_entity_id.lispb) · [External scalar registry](../../lispb/schema/types.lispb) |
| Kernel | Declare operation, operand storage, type sets, and aliasing in a [kernel module](../core/Kernels/vector_lerp.lispb). |
| Slate | Declare the static widget tree and slots. Keep dynamic state and event handling in the host. |
| Material | Edit the [source graph](../../Plugins/SandboxUI/Source/SandboxUI/Private/materials/UiGlowComposite.lispb). For UI glow, run `coj unreal generate-ui-glow-material`. See `coj unreal --help` for other assets. |

SoA views borrow their owner's columns. Resolve a view once before a hot loop and reacquire it after storage moves. Inspect physical layout with [Layout Planner](../../tools/layout_planner/README.md).

## S7 records and settings

```lisp
(record Weapon :generate-s7-reader true :s7-tag "weapon"
  (member damage float :initializer 5.f :nonnegative true)
  (member count int :required true)
  (projections (cpp)))
```

Reading `(:weapon :count 2)` keeps the default damage of 5. A fresh read requires `count`. `read_override` starts from an existing record. Required fields may be omitted in an override. Omitted fields retain their values, nested records merge, and collections replace the whole field. Unknown and duplicate names fail validation. Use `:s7-exclude true` for C++-only fields and `:generate-s7-writer true` for serialization. [Level inputs](../../lispb/schema/level_inputs.lispb) · [Settings](../../lispb/schema/settings.lispb)

## Enum helpers

Use `:helpers` for identifiers, serialized names, display labels, and formatting. String tables require unique dense values starting at zero. Mark enum members with groups to generate membership predicates and ordered value arrays. [Simulation enums](../../lispb/schema/native_simulation_enums.lispb)

## Editor installation on Windows x64

Run either command from a prepared worktree with Node.js, npm and the .NET SDK installed.

- `cmake --workflow --preset lispb-editors` builds and tests both extensions headlessly with the Debug server and prints the absolute VSIX path for each editor.
- `python native/lispb/lsp/editors/install.py --workflow` runs the `install-lispb-editors` CMake workflow preset and prints any blocked-install hint after Ninja exits. The preset builds and tests both extensions with the Release server, then installs them for the current user using `code.cmd` from PATH and the latest Visual Studio 2026 IDE found by `vswhere`.

Save your work and close the target Visual Studio IDE before installing. Exit 2004 means a running process blocked the change. The workflow prints the blockers from the installer log and saves a copy beside the VSIX.

For manual installation, select the printed VS Code package in **Extensions → Install from VSIX**, or open the Visual Studio 2026 package with VSIX Installer. Open a `.lispb` file to activate the extension.
