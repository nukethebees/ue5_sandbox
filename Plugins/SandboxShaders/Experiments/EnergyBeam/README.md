# Energy Beam

Draw an additive beam between two actor-local points. Set `source_offset` and `destination_offset`
on `AEnergyBeamExperimentActor`. The actor aligns a cylinder between them. `EnergyBeam.ush` projects
each pixel onto the world-space segment to place moving packets and brighten the ends.

Select **Energy Beam** in the [shader showcase](../../README.md). Use `set_short_beam`,
`set_long_beam`, or `reverse_direction`. Adjust width, flow speed, turbulence, colours, and emission
live. Pause or restart the animation. The beam is visual only.
