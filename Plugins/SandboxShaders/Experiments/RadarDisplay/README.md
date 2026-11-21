# Procedural Radar / Sensor Display

Render range rings, a rotating sweep, and four pulsing contacts on a texture-free surface. Edit the
four contacts, colours, sweep, rings, interference, and emission on `ARadarDisplayExperimentActor`.
Contacts are fixed material parameters.

`M_RadarDisplay` calls `Shaders/Private/RadarDisplay/RadarDisplay.ush`. Polar coordinates place
rings and sweep, masks draw the contacts and axes, and scanlines add interference. Material Time
animates the display in the editor and at runtime.
