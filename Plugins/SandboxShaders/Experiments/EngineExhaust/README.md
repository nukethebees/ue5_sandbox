# Procedural Engine Exhaust

Render a plasma plume on a two-sided additive cone. In the [shader showcase](../../README.md),
select **ENGINE EXHAUST**. Edit `Throttle`, or use **Set Idle**, **Set Half Throttle**, or
**Set Full Throttle**. Throttle changes cone length, radius, and plasma intensity.

`AEngineExhaustExperimentActor` drives time, flow, turbulence, emission, and temperature colours.
`Shaders/Private/EngineExhaust/EngineExhaust.ush` uses animated UV noise, falloff, and flicker to
shape the tail.
