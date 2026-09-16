# Unreal source modules

`Source/` contains the project modules declared by `Sandbox.uproject`:

| Module | Responsibility |
| --- | --- |
| `Sandbox` | Runtime game module and core gameplay integration. |
| `SandboxNative` | Runtime adapters for standalone native libraries. |
| `SandboxEditor` | Editor-only game and authoring support. |
| `SandboxTests` | Editor test support for the game module. |
| `SandboxEditorTests` | Editor-specific test coverage. |

Reusable or independently enabled Unreal extensions belong in [Plugins](../Plugins/README.md).
Keep simulation-heavy logic in `native/` until an engine dependency requires an adapter. Build and
test these modules through the [documented CMake workflows](../docs/build-and-test.md).
