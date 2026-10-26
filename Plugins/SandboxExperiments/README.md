# Sandbox Experiments

Optional UI and shader experiments, showcases, benchmarks and shader smoke tests.
Disabled in normal builds. Enable `SandboxExperiments` in `Sandbox.uproject`, then
build with `cmake --build --preset debug-game` before opening its showcase assets
or running `coj unreal generate-slate-dsl-smoke-asset`.

Module names are unchanged. Showcase assets live under
`/SandboxExperiments/UIExamples` and `/SandboxExperiments/ShaderShowcase`, so they
are not mounted when the plugin is disabled. Package redirects preserve references
from existing assets. Shared materials, shader files and production rendering stay
in `SandboxUI` and `SandboxShaders`.
