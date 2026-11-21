# Space Dust

Open **Development → Space Dust Flight Lab** from level select. Fly past the friendly capital ship
to check motion and occlusion. Dust appears in the player’s gameplay camera. Observer views,
scene captures, reflections, and ray tracing omit it.

## Tuning

Adjust `FPlayerShipConfig::space_dust`. Set count, seed, volume dimensions, thickness, brightness,
RGB colour, and edge fade. Set streak duration and maximum pixel length to control the trails.

| Control | Default or limit |
| --- | --- |
| Candidate count | 96 by default, 2,048 maximum |
| Apparent-motion fade | Invisible at 0.75 pixels, fully visible at 4 pixels (1080-pixel reference height) |
| World-speed fade | 100 to 2,000 world units/s |

In non-shipping builds, use `space_dust.preset off`, `space_dust.preset default`, or
`space_dust.preset strong`. Strong keeps the level’s volume and colour, uses 192 candidates
and a 0.5–3 pixel fade, and increases brightness and streak duration.

## Flight checks

| Action | Expected result |
| --- | --- |
| Stop or rotate in place | Dust fades out. At very low speed, only occasional particles appear. |
| Drift sideways, then turn | Streaks follow world velocity through the turn. |
| Accelerate forward | Streaks spread radially, strongest nearby. The field stays sparse at high speed. |
| Combine forward and sideways motion | Streak direction varies across the view. |
| Approach the hull | Opaque geometry occludes dust. Particles fade as they cross the camera plane. |
| Travel far, teleport, or reset | Dust wraps smoothly at the new position. |
| Change resolution | Apparent density and streak proportions stay consistent. |

## Renderer and measurement

`USpaceDustComponent` submits one instanced quad draw per visible owner view. The vertex shader
seeds positions from `SV_InstanceID` and wraps them around the camera using its large-world
position. It projects each particle twice using the ship’s simulation velocity and
`streak_seconds`. The screen-space difference drives streak length, direction, and visibility.

Compare disabled, 96, and 384 candidates in Unreal Insights or CSV. Check render-thread
submission under `SpaceDustSubmit`, then compare GPU time and covered pixels. Submission
cost should stay roughly constant. [Motion and settings tests](../../../native/gfx/tests/space_dust_math_tests.cpp)
