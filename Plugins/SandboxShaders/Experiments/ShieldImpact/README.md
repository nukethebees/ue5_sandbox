# Shield Impact / Force Field

This experiment demonstrates a localized, event-driven surface effect on an additive shield. The
pixel shader measures geodesic distance across the sphere from an editable local impact direction,
which keeps the expanding ripple circular on curved geometry. Fresnel lighting, procedural noise,
and a decaying impact glow provide the surrounding force-field response.

The Custom expression in `M_ShieldImpact` includes
`Shaders/Private/ShieldImpact/ShieldImpact.ush`. `AShieldImpactExperimentActor` creates a dynamic
material instance, forwards its editable settings, and advances the normalized `ImpactPhase` in
runtime and editor viewport ticks. Use **Trigger Impact** in the Details panel, call
`trigger_impact_at_local_position`, or leave auto-repeat enabled for the showcase.

The impact centre is a normalized local-space direction rather than a collision result. No scene
refraction, collision query, replication, or multi-impact history is implemented.
