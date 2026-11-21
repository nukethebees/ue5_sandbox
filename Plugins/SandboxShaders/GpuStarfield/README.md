# GPU Starfield

GPU Starfield is a deterministic, camera-relative background renderer. Gameplay uses the loaded mission's `starfield` settings. For standalone editor preview, the actor reads `settings_file` when its components register or when you click **Load Preview Settings**. Colours remain actor properties. Settings layers apply in order, so an override can change only the required fields:

```scheme
(export game-settings)
(define game-settings
  (settings-layers
    (module-ref (import-module "Settings/level-defaults.scm") 'game-settings)
    (settings :starfield (starfield :generation (generation :star-count 100000)))))
```

Run the CMake benchmark or matrix workflow for target-hardware evidence.

See [ARCHITECTURE.md](ARCHITECTURE.md) for the scene-proxy pipeline, shader contract, culling limits,
and performance model.
