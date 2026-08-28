# Shield Impact / Force Field

This experiment demonstrates a localized, event-driven surface effect on an additive shield. Up to
four simultaneous impacts carry a normalized local-space position, starting radius, age, and
strength. The pixel shader measures geodesic distance across the sphere, keeping each expanding
ripple circular on curved geometry. Fresnel lighting, procedural noise, local hex activation, and a
decaying flash provide the surrounding force-field response.

The Custom expression in `M_ShieldImpact` includes
`Shaders/Private/ShieldImpact/ShieldImpact.ush`. `AShieldImpactExperimentActor` creates a dynamic
material instance and owns a fixed four-entry CPU event buffer. Every tick it packs four
`ImpactDataN` vectors (position plus radius) and four `ImpactStateN` vectors (age, strength, active)
into material parameters. Use **Trigger Impact**, call `add_impact` or
`trigger_impact_at_local_position`, or leave auto-repeat enabled. When full, a new event replaces
the oldest; **Clear Impacts** resets the buffer.

The impact centre is a normalized local-space direction rather than a collision result. A gameplay
hit could transform its world-space impact point into the shield actor's local space before calling
`add_impact`. No collision query, replication, or generalized damage system is implemented.
