# SandboxUI Shader Development

* Do not use HLSL reserved words as identifiers. In particular, `point` is an input-primitive
  modifier and cannot be used as a function parameter name. DXC can report the misleading
  `modifiers must appear before type` diagnostic at that identifier, followed by many cascading
  parse errors. Prefer names such as `sample_position` or `pixel_position`.
* Validate new or changed global shaders with a real graphics RHI (normally PCD3D_SM6). Null RHI
  build and test workflows can load the module without compiling the shader permutation that the
  editor uses.
