# SandboxShaders

`SandboxShaders` is a deliberately small shader playground. The runtime `SandboxShaders` module
only installs the `/Plugin/SandboxShaders` shader-source mapping. `SbxShadersExperiments` contains
the concrete experiment actors and their rendering code, while `SandboxShadersEditor` adds the
showcase launcher and smoke tests.

The experiments module uses the qualified name `SbxShadersExperiments` because this project already
has an Unreal module named `Experiments` in `SandboxUI`; Unreal module names share one target-wide
namespace. Keeping the existing module intact avoids modifying an unrelated plugin while preserving
an unmistakable experiments module here.

Open `/SandboxShaders/Showcase/SandboxShaders_Showcase`, or use **Window > Sandbox Shaders > Open
Shader Showcase** in the editor. Select an experiment actor to edit its exposed settings.

See the experiment-specific documentation:

- [Procedural Energy Shield](Experiments/EnergyShield/README.md)
- [Procedural Space / Energy Field](Experiments/SpaceEnergyField/README.md)
- [Shield Impact / Force Field](Experiments/ShieldImpact/README.md)
- [Vertex Ripple / World Position Offset](Experiments/VertexRipple/README.md)
- [Procedural Radar / Sensor Display](Experiments/RadarDisplay/README.md)
- [Tactical Scan / Scene-Depth Post Process](Experiments/TacticalScan/README.md)
- [Warp / Distortion Field](Experiments/WarpField/README.md)
- [Raymarched Energy Anomaly](Experiments/RaymarchedAnomaly/README.md)
- [Procedural Engine Exhaust](Experiments/EngineExhaust/README.md)
- [Planet Atmosphere](Experiments/PlanetAtmosphere/README.md)
- [Construction / Spawn](Experiments/ConstructionSpawn/README.md)
- [Energy Beam](Experiments/EnergyBeam/README.md)
