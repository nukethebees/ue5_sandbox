(material M_UiGlowComposite
  (asset "/SandboxUI/Generated/Materials/M_UiGlowComposite")
  (domain ui)
  (blend additive)

  (parameter texture GlowEnergy "/SandboxUI/Materials/T_UiGlowBlack")
  (parameter texture CoreTexture "/SandboxUI/Materials/T_UiGlowBlack")
  (parameter color GlowColor 0.84 0.65 0.23 1.0)
  (parameter scalar PreserveCorePixels 0.0)

  (let uv (texcoord 0))

  (emissive
    (custom float3
      ((Energy texture GlowEnergy)
       (Core texture CoreTexture)
       (UV float2 uv)
       (Color float4 GlowColor)
       (Preserve float PreserveCorePixels))
      :description "Additive glow with optional strict core-pixel preservation"
      "float core_alpha = Texture2DSample(Core, CoreSampler, UV).a;\n"
      "float energy = Texture2DSample(Energy, EnergySampler, UV).r;\n"
      "return Preserve > 0.5 && core_alpha > 0.0 ? float3(0, 0, 0) : energy * Color.rgb;\n")))
