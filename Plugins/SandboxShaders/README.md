# Sandbox shaders

## Runtime systems

| System | Use |
| --- | --- |
| GPU Starfield | Set `starfield` in mission settings. For an editor preview, set `settings_file` on the actor and click **Load Preview Settings**. |
| Celestial Backdrop | Place `ACelestialBackdropActor`, set radius and sun direction, then assign an appearance profile. Layered mode supports opaque depth. Analytic mode draws one translucent proxy. |
| Space Dust | Tune the player camera's velocity streaks in `FPlayerShipConfig::space_dust`. Open **Development → Space Dust Flight Lab**. In a non-shipping build, try `space_dust.preset off`, `default`, or `strong`. |

## Shader experiments

[Open the showcase](../SandboxExperiments/README.md), then select an effect.

| Effect | What to look for |
| --- | --- |
| Construction / Spawn | A noisy masked edge reveals a cube as progress advances. |
| Energy Beam | An additive beam carries bright packets between two endpoints. |
| Energy Shield | A procedural force field wraps a sphere. |
| Engine Exhaust | Animated noise shapes a plasma plume on a cone. |
| Nebula Backdrop | Four layers on one card shift with the camera. |
| Nebula Volume | A raymarched cloud clips against scene depth. Quality controls step count. |
| Planet Atmosphere | Two spheres form the day/night surface and its sunlit rim. |
| Radar Display | Polar coordinates draw rings, sweep, and contacts on a surface. |
| Raymarched Anomaly | A material raymarches a noisy sphere and torus on a quad. |
| Shield Impact | Up to four expanding ripples follow the shield surface. |
| Space Energy Field | A global compute shader generates an animated HDR backdrop. |
| Tactical Scan | A post-process scan plane reveals a grid on scene geometry. |
| Vertex Ripple | One traveling wave drives mesh displacement and emissive color. |
| Warp Field | Animated noise modulates refraction across a translucent sphere. |
