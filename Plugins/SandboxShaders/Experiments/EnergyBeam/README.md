# Energy Beam

## What it demonstrates

This experiment creates a procedural additive beam between two C++-supplied endpoints. It combines
a bright view-facing core, turbulent edge sheath, packets moving along the beam, and extra energy at
the source and destination.

## Rendering pipeline

`AEnergyBeamExperimentActor` treats `source_offset` and `destination_offset` as actor-local points.
C++ places a cheap engine cylinder at their midpoint, rotates its local Z axis onto the endpoint
direction, and scales it to the requested width and distance. The same endpoints are transformed to
world space and written to the dynamic material instance.

`EnergyBeam.ush` projects each pixel's world position onto the supplied beam segment to obtain a
normalized longitudinal coordinate. That coordinate drives packet flow, endpoint emphasis, and
procedural noise. Surface normal versus camera direction separates the apparent core from the
sheath. The result reaches the screen through an unlit additive material.

## Editor controls and showcase

Open `/SandboxShaders/Showcase/SandboxShaders_Showcase` and select `Energy Beam`. Edit either
endpoint or use `set_short_beam`, `set_long_beam`, and `reverse_direction`. Width, flow speed,
turbulence, colours, and emission update live; animation can be paused or restarted.

## Performance and limitations

The implementation is one cylinder and a bounded procedural material. It is not camera-facing
geometry, does not perform collision or weapon damage, and renders endpoint emphasis on the beam
surface rather than spawning separate impact geometry. A gameplay beam can later supply its traced
source/destination without changing the shader interface.
