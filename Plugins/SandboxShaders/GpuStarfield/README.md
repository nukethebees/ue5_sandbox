# GPU Starfield

GPU Starfield is a deterministic, camera-relative background renderer. Numerical settings come from
the actor's `settings_file`, defaulting to `LevelScripts/Settings/starfield.scm`. Colours remain actor
properties. Use **Apply Settings** to reload a script in the editor; actor construction also reloads
it. Settings layers apply in order, so an override can change only the required fields:

```scheme
(settings-layers
  (load-script "Settings/starfield.scm")
  (starfield :generation (generation :star-count 100000)))
```

Run the CMake benchmark or matrix workflow for target-hardware evidence.

See [ARCHITECTURE.md](ARCHITECTURE.md) for the scene-proxy pipeline, shader contract, culling limits,
and performance model.
