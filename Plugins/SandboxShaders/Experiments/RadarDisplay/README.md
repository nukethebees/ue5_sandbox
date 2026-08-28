# Procedural Radar / Sensor Display

This experiment demonstrates texture-free 2D procedural graphics in a surface material. Polar
coordinates form the range rings and rotating sweep, while signed-distance-style masks produce the
rim, axes, sweep trail, and four pulsing contacts. Hash noise and high-frequency scanlines add
controlled interference.

The Custom expression in `M_RadarDisplay` includes
`Shaders/Private/RadarDisplay/RadarDisplay.ush`. `ARadarDisplayExperimentActor` converts four
editable contact structs into material vector parameters and also forwards the theme colours,
sweep, ring, interference, and emissive controls. Unreal's material Time input animates the sweep
in editor and runtime views.

Contacts are deliberately fixed authoring parameters rather than live sensor data. The experiment
does not implement UI hit testing, render targets, persistence, or a generalized contact buffer.
