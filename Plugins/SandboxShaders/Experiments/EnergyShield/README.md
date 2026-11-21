# Procedural Energy Shield

Render a cyan shield with a triplanar hex grid, scanning band, and Fresnel rim. Open the
[shader showcase](../../README.md) and select the sphere on the left. Edit `FEnergyShieldSettings`
on `AEnergyShieldExperimentActor` to preview changes in the viewport.

`M_EnergyShield` calls `sbx_energy_shield` in
`/Plugin/SandboxShaders/Private/EnergyShield/EnergyShield.ush`. RGB drives emission and alpha drives
opacity. Noise distorts the grid coordinates. The translucent sphere requires SM5 or newer and
incurs sorting and overdraw costs.
