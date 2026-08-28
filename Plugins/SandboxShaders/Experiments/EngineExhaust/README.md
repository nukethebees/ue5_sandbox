# Procedural Engine Exhaust

This experiment uses a cheap two-sided cone as the carrier for an additive procedural plasma
material. `Shaders/Private/EngineExhaust/EngineExhaust.ush` combines UV-aligned FBM, downstream flow,
turbulent lateral variation, tail falloff, and flicker to produce a bright core and broken-up tail.

`AEngineExhaustExperimentActor` creates the dynamic material instance, advances an explicit clock,
and sends throttle, thrust, flow, turbulence, emission, and temperature colours to the shader.
Throttle also scales the supporting cone's length and radius, making the CPU/editor control visibly
affect both the carrier and GPU energy response.

Open the showcase and select **ENGINE EXHAUST**. Edit `Throttle` directly or use **Set Idle**, **Set
Half Throttle**, and **Set Full Throttle**. This is a surface-based additive illusion: it has no
Niagara particles, soft-particle depth fade, volumetric shadowing, or ship integration.
