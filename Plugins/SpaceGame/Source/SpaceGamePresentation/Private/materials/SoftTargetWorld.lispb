(material M_SoftTargetWorld
  (asset "/SpaceGame/Generated/Materials/M_SoftTargetWorld")
  (domain surface)
  (blend translucent)
  (shading unlit)
  (two-sided true)
  (disable-depth-test true)
  (usage instanced-static-meshes)

  (let red (per-instance-custom-data 0))
  (let green (per-instance-custom-data 1))
  (let blue (per-instance-custom-data 2))
  (let opacity (per-instance-custom-data 3))
  (let intensity (per-instance-custom-data 4))
  (let range_alpha (per-instance-custom-data 5))

  (emissive
    (custom float3
      ((Red float red)
       (Green float green)
       (Blue float blue)
       (Intensity float intensity)
       (RangeAlpha float range_alpha))
      :description "Per-instance soft-target colour, intensity, and range alpha"
      "float clamped_range_alpha = saturate(RangeAlpha);\n"
      "float range_intensity = lerp(1.0, 2.0, clamped_range_alpha);\n"
      "return float3(Red, Green, Blue) * Intensity * range_intensity * 2.0;\n"))
  (opacity opacity))
