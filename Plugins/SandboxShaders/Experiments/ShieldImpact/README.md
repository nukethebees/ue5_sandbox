# Shield Impact / Force Field

Send up to four circular ripples across a spherical shield. Use **Trigger Impact**, enable
auto-repeat, or call `add_impact` or `trigger_impact_at_local_position`. Supply a normalized
shield-local direction. The fifth hit replaces the oldest. Use **Clear Impacts** to reset.

`AShieldImpactExperimentActor` sends four pairs of material vectors each tick. `ImpactDataN` holds
position and radius. `ImpactStateN` holds age, strength, and active state. `M_ShieldImpact` calls
`Shaders/Private/ShieldImpact/ShieldImpact.ush` to draw each ripple by distance along the sphere.
