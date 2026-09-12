#include <material_gen/CompiledMaterial.h>
#include <material_gen/MaterialFrontend.h>
#include <material_gen/SourceHash.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <sstream>
#include <vector>

namespace material_synth {
namespace {

TEST(MaterialGen, ComputesSha256) {
    constexpr std::string_view input{"abc"};
    auto const bytes{std::span{reinterpret_cast<std::uint8_t const*>(input.data()), input.size()}};
    EXPECT_EQ(sha256(bytes), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

auto resolve(void const*, std::string_view const path) -> std::optional<std::string> {
    auto const slash{path.rfind('/')};
    if (path.starts_with("/Missing/") || slash == std::string_view::npos) {
        return std::nullopt;
    }
    return std::string{path} + "." + std::string{path.substr(slash + 1)};
}

auto read(std::filesystem::path const& path) -> std::string {
    std::ifstream stream{path};
    std::ostringstream contents;
    contents << stream.rdbuf();
    return contents.str();
}

TEST(MaterialFrontend, LowersUiGlowGoldenSourceWithStableHandles) {
    auto const source_path{std::filesystem::path{SANDBOX_PROJECT_SOURCE_DIR} /
                           "Plugins/SandboxUI/Source/SandboxUI/Private/materials/"
                           "UiGlowComposite.lispb"};
    auto const result{analyze(
        source_path.generic_string(), read(source_path), TextureResolver{nullptr, resolve})};
    ASSERT_TRUE(result.material.has_value())
        << (result.diagnostics.empty() ? "" : result.diagnostics.front().message);
    auto const& material{*result.material};
    EXPECT_EQ(material.settings.name, "M_UiGlowComposite");
    EXPECT_EQ(material.settings.package_path, "/SandboxUI/Generated/Materials/M_UiGlowComposite");
    ASSERT_EQ(material.parameters.size(), 4);
    for (std::size_t index{}; index < material.parameters.size(); ++index) {
        EXPECT_EQ(material.parameters[index].node.index, index);
    }
    EXPECT_EQ(material.nodes[4].kind, NodeKind::texture_coordinate);
    EXPECT_EQ(std::ranges::count_if(material.nodes,
                                    [](Node const& node) { return node.kind == NodeKind::sample; }),
              2);
    auto const custom{std::ranges::find_if(
        material.nodes, [](Node const& node) { return node.kind == NodeKind::custom; })};
    ASSERT_NE(custom, material.nodes.end());
    EXPECT_EQ(custom->type, ValueType::float1);
    ASSERT_EQ(custom->custom_inputs.size(), 2);
    EXPECT_EQ(custom->custom_inputs[1].name, "CoreAlpha");
    EXPECT_EQ(material.nodes[material.outputs.front().node.index].kind, NodeKind::multiply);
    EXPECT_EQ(material.texture_dependencies.size(), 1);
    EXPECT_TRUE(validate(material).empty());
}

TEST(CompiledMaterial, RoundTripsDeterministically) {
    auto const source_path{std::filesystem::path{SANDBOX_PROJECT_SOURCE_DIR} /
                           "Plugins/SandboxUI/Source/SandboxUI/Private/materials/"
                           "UiGlowComposite.lispb"};
    auto const source{read(source_path)};
    auto analysis{analyze(source_path.generic_string(), source, TextureResolver{nullptr, resolve})};
    ASSERT_TRUE(analysis.material.has_value());
    auto const source_bytes{
        std::span{reinterpret_cast<std::uint8_t const*>(source.data()), source.size()}};
    CompiledMaterial const compiled{.source_path =
                                        "Plugins/SandboxUI/Source/SandboxUI/Private/materials/"
                                        "UiGlowComposite.lispb",
                                    .source_hash = sha256(source_bytes),
                                    .material = std::move(*analysis.material)};

    auto const first{serialize(compiled)};
    auto const second{serialize(compiled)};
    ASSERT_TRUE(first.has_value()) << first.error();
    ASSERT_TRUE(second.has_value()) << second.error();
    EXPECT_EQ(*first, *second);

    auto const decoded{deserialize(*first)};
    ASSERT_TRUE(decoded.has_value()) << decoded.error();
    EXPECT_EQ(decoded->source_path, compiled.source_path);
    EXPECT_EQ(decoded->source_hash, compiled.source_hash);
    EXPECT_EQ(decoded->material.settings.name, "M_UiGlowComposite");
    EXPECT_EQ(decoded->material.parameters.size(), 4);
    EXPECT_TRUE(std::ranges::any_of(decoded->material.nodes, [](Node const& node) {
        return node.kind == NodeKind::component_mask;
    }));
    EXPECT_TRUE(validate(decoded->material).empty());
}

TEST(CompiledMaterial, RejectsWrongVersionCorruptionAndTrailingData) {
    std::vector<std::uint8_t> wrong_version{'S', 'B', 'X', 'M', 'A', 'T', 'I', 'R', 6, 0, 0, 0};
    EXPECT_FALSE(deserialize(wrong_version).has_value());

    std::vector<std::uint8_t> truncated{'S', 'B', 'X'};
    EXPECT_FALSE(deserialize(truncated).has_value());

    auto analysis{
        analyze("source.scm",
                "(material M (asset \"/Game/Generated/Materials/M\") (domain ui) (blend additive) "
                "(emissive (float3 1 1 1)))",
                TextureResolver{nullptr, resolve})};
    ASSERT_TRUE(analysis.material.has_value());
    auto artifact{serialize(
        {.source_path = "source.scm",
         .source_hash = "0000000000000000000000000000000000000000000000000000000000000000",
         .material = std::move(*analysis.material)})};
    ASSERT_TRUE(artifact.has_value());
    artifact->push_back(0);
    EXPECT_FALSE(deserialize(*artifact).has_value());
}

TEST(MaterialFrontend, LowersWorldSurfaceSettingsInstanceDataAndOpacity) {
    auto const source_path{std::filesystem::path{SANDBOX_PROJECT_SOURCE_DIR} /
                           "Plugins/SpaceGame/Source/SpaceGamePresentation/Private/materials/"
                           "SoftTargetWorld.lispb"};
    auto const result{analyze(
        source_path.generic_string(), read(source_path), TextureResolver{nullptr, resolve})};
    ASSERT_TRUE(result.material.has_value())
        << (result.diagnostics.empty() ? "" : result.diagnostics.front().message);
    auto const& material{*result.material};
    EXPECT_EQ(material.settings.domain, MaterialDomain::surface);
    EXPECT_EQ(material.settings.blend_mode, BlendMode::translucent);
    EXPECT_EQ(material.settings.shading_model, ShadingModel::unlit);
    EXPECT_TRUE(material.settings.two_sided);
    EXPECT_TRUE(material.settings.disable_depth_test);
    EXPECT_TRUE(material.settings.used_with_instanced_static_meshes);
    ASSERT_EQ(material.parameters.size(), 16);
    auto const filaments{
        std::ranges::find(material.parameters, "EnergyFilaments", &Parameter::name)};
    ASSERT_NE(filaments, material.parameters.end());
    EXPECT_EQ(filaments->texture_sampler_type, TextureSamplerType::linear_grayscale);
    auto const flow{std::ranges::find(material.parameters, "EnergyFlow", &Parameter::name)};
    ASSERT_NE(flow, material.parameters.end());
    EXPECT_EQ(flow->texture_sampler_type, TextureSamplerType::linear_color);
    for (unsigned instance_data_index{}; instance_data_index < 6; ++instance_data_index) {
        EXPECT_TRUE(std::ranges::any_of(material.nodes, [&](Node const& node) {
            return node.kind == NodeKind::per_instance_custom_data &&
                   node.instance_data_index == instance_data_index;
        }));
    }
    EXPECT_TRUE(std::ranges::any_of(material.nodes,
                                    [](Node const& node) { return node.kind == NodeKind::time; }));
    EXPECT_TRUE(std::ranges::any_of(
        material.nodes, [](Node const& node) { return node.kind == NodeKind::saturate; }));
    EXPECT_TRUE(std::ranges::any_of(material.nodes,
                                    [](Node const& node) { return node.kind == NodeKind::lerp; }));
    EXPECT_EQ(std::ranges::count_if(material.nodes,
                                    [](Node const& node) { return node.kind == NodeKind::custom; }),
              3);
    ASSERT_EQ(material.outputs.size(), 2);
    EXPECT_EQ(material.outputs[1].name, "opacity");
    EXPECT_TRUE(validate(material).empty());

    CompiledMaterial const compiled{
        .source_path = source_path.generic_string(),
        .source_hash = "0000000000000000000000000000000000000000000000000000000000000000",
        .material = material};
    auto const bytes{serialize(compiled)};
    ASSERT_TRUE(bytes.has_value()) << bytes.error();
    auto const decoded{deserialize(*bytes)};
    ASSERT_TRUE(decoded.has_value()) << decoded.error();
    EXPECT_EQ(decoded->material.settings.domain, MaterialDomain::surface);
    EXPECT_EQ(decoded->material.settings.shading_model, ShadingModel::unlit);
    EXPECT_TRUE(decoded->material.settings.used_with_instanced_static_meshes);
    EXPECT_EQ(decoded->material.parameters.size(), 16);
    EXPECT_TRUE(std::ranges::any_of(decoded->material.nodes,
                                    [](Node const& node) { return node.kind == NodeKind::time; }));
}

TEST(MaterialFrontend, LowersTimeTrigDynamicVectorsAndNaryArithmetic) {
    constexpr std::string_view source{R"(
(material M_Pulse
  (asset "/Game/Generated/Materials/M_Pulse") (domain ui) (blend additive)
  (parameter color Colour 0.8 0.6 0.2 1.0)
  (parameter scalar Speed 2.0)
  (parameter scalar Intensity 4.0)
  (let phase (* (time) Speed))
  (let pulse (* 0.5 (+ 1.0 (sin phase))))
  (let offset (cos phase))
  (let tint (float3 pulse offset 1.0))
  (emissive (* tint Intensity 1.0 1.0))))"};
    auto const result{analyze("pulse.scm", source, TextureResolver{nullptr, resolve})};
    ASSERT_TRUE(result.material.has_value())
        << (result.diagnostics.empty() ? "" : result.diagnostics.front().message);
    auto const& material{*result.material};

    auto const find_binding{[&](std::string_view const name) -> Node const& {
        auto const binding{
            std::ranges::find_if(material.bindings, [name](NamedNode const& candidate) {
                return candidate.name == name;
            })};
        EXPECT_NE(binding, material.bindings.end());
        return material.nodes[binding->node.index];
    }};

    EXPECT_EQ(find_binding("phase").kind, NodeKind::multiply);
    EXPECT_EQ(find_binding("pulse").kind, NodeKind::multiply);
    EXPECT_EQ(find_binding("offset").kind, NodeKind::cosine);
    EXPECT_EQ(find_binding("tint").kind, NodeKind::vector_constructor);
    EXPECT_TRUE(std::ranges::any_of(material.nodes,
                                    [](Node const& node) { return node.kind == NodeKind::time; }));
    EXPECT_TRUE(std::ranges::any_of(material.nodes,
                                    [](Node const& node) { return node.kind == NodeKind::sine; }));
    auto const& emissive{material.nodes[material.outputs.front().node.index]};
    EXPECT_EQ(emissive.kind, NodeKind::multiply);
    EXPECT_EQ(emissive.inputs.size(), 4);
    EXPECT_TRUE(validate(material).empty());
}

TEST(MaterialFrontend, SupportsEveryNumericExpressionAndPropagatesTypes) {
    constexpr std::string_view source{R"(
(material M_Forms
  (asset "/Game/Generated/Materials/M_Forms") (domain ui) (blend additive)
  (parameter texture Tex "/Game/T")
  (let uv (texcoord 1))
  (let two (float2 1 2))
  (let three (float3 1 2 3))
  (let four (float4 1 2 3 4))
  (let add (+ three 1))
  (let sub (- add 1))
  (let mul (* sub 2))
  (let div (/ mul 2))
  (let mixed (lerp div three 0.5))
  (let clipped (saturate mixed))
  (let sampled (sample Tex uv))
  (emissive (custom float3 ((Value float3 clipped) (Pixels float4 sampled))
              "return Value + Pixels.rgb * 0;"))))"};
    auto const result{analyze("forms.scm", source, TextureResolver{nullptr, resolve})};
    ASSERT_TRUE(result.material.has_value())
        << (result.diagnostics.empty() ? "" : result.diagnostics.front().message);
    auto const& material{*result.material};
    EXPECT_EQ(material.nodes[2].type, ValueType::float2);
    EXPECT_EQ(material.nodes[3].type, ValueType::float3);
    EXPECT_EQ(material.nodes[4].type, ValueType::float4);
    auto const sampled{std::ranges::find_if(
        material.bindings, [](NamedNode const& binding) { return binding.name == "sampled"; })};
    ASSERT_NE(sampled, material.bindings.end());
    EXPECT_EQ(material.nodes[sampled->node.index].kind, NodeKind::sample);
    EXPECT_EQ(material.nodes[sampled->node.index].type, ValueType::float4);
}

TEST(MaterialFrontend, LowersMigratedMaterialsToNativeScaffoldingAndTypedShaderLeaves) {
    auto const source_root{std::filesystem::path{SANDBOX_PROJECT_SOURCE_DIR}};
    std::array<std::filesystem::path, 9> const sources{
        source_root / "Plugins/SandboxShaders/Source/SbxShadersExperiments/Private/materials/"
                      "RadarDisplay.lispb",
        source_root / "Plugins/SandboxShaders/Source/SbxShadersExperiments/Private/materials/"
                      "EnergyShield.lispb",
        source_root / "Plugins/SandboxShaders/Source/SbxShadersExperiments/Private/materials/"
                      "SpaceEnergyFieldDisplay.lispb",
        source_root / "Source/Sandbox/materials/GlowingCross.lispb",
        source_root / "Plugins/SandboxShaders/Source/SbxShadersExperiments/Private/materials/"
                      "VertexRipple.lispb",
        source_root / "Plugins/SandboxShaders/Source/SbxShadersExperiments/Private/materials/"
                      "ConstructionSpawn.lispb",
        source_root / "Plugins/SandboxShaders/Source/SbxShadersExperiments/Private/materials/"
                      "PlanetAtmosphere.lispb",
        source_root / "Plugins/SandboxShaders/Source/SbxShadersExperiments/Private/materials/"
                      "PlanetSurface.lispb",
        source_root / "Plugins/SandboxShaders/Source/SbxShadersExperiments/Private/materials/"
                      "TacticalScan.lispb"};

    std::vector<MaterialIR> materials;
    for (auto const& source_path : sources) {
        auto result{analyze(
            source_path.generic_string(), read(source_path), TextureResolver{nullptr, resolve})};
        ASSERT_TRUE(result.material.has_value())
            << source_path << ": "
            << (result.diagnostics.empty() ? "" : result.diagnostics.front().message);
        EXPECT_TRUE(result.material->settings.adopt_existing);
        EXPECT_TRUE(validate(*result.material).empty());
        materials.push_back(std::move(*result.material));
    }

    auto const& energy_shield{materials[1]};
    EXPECT_TRUE(std::ranges::any_of(
        energy_shield.nodes, [](Node const& node) { return node.kind == NodeKind::subtract; }));
    EXPECT_TRUE(std::ranges::any_of(energy_shield.nodes, [](Node const& node) {
        return node.kind == NodeKind::shader_call && node.shader_function == "sbx_energy_shield";
    }));

    auto const& glowing_cross{materials[3]};
    EXPECT_TRUE(std::ranges::none_of(glowing_cross.nodes, [](Node const& node) {
        return node.kind == NodeKind::custom || node.kind == NodeKind::shader_call;
    }));
    EXPECT_TRUE(std::ranges::any_of(glowing_cross.nodes, [](Node const& node) {
        return node.kind == NodeKind::texture_object;
    }));

    auto const& construction_spawn{materials[5]};
    EXPECT_EQ(construction_spawn.settings.blend_mode, BlendMode::masked);
    EXPECT_TRUE(std::ranges::any_of(construction_spawn.nodes, [](Node const& node) {
        return node.kind == NodeKind::transform_position;
    }));
    EXPECT_TRUE(std::ranges::any_of(construction_spawn.outputs, [](NamedNode const& output) {
        return output.name == "opacity-mask";
    }));

    auto const& tactical_scan{materials[8]};
    EXPECT_EQ(tactical_scan.settings.domain, MaterialDomain::post_process);
    EXPECT_EQ(std::ranges::count_if(
                  tactical_scan.nodes,
                  [](Node const& node) { return node.kind == NodeKind::scene_texture; }),
              2);
}

TEST(MaterialIR, RejectsForgedForwardHandlesAndMalformedNodes) {
    MaterialIR material;
    material.settings.name = "M_Invalid";
    material.settings.package_path = "/Game/Generated/Materials/M_Invalid";
    material.nodes.push_back(
        Node{.kind = NodeKind::add, .type = ValueType::float1, .inputs = {{1}, {1}}});
    material.outputs.push_back({"emissive", {0}, {}});
    EXPECT_FALSE(validate(material).empty());
}

class MaterialFrontendFailure : public testing::TestWithParam<std::string_view> {};

TEST_P(MaterialFrontendFailure, RejectsInvalidSource) {
    auto const result{analyze("invalid.scm", GetParam(), TextureResolver{nullptr, resolve})};
    EXPECT_FALSE(result.material.has_value());
    EXPECT_FALSE(result.diagnostics.empty());
    if (!result.diagnostics.empty()) {
        if (!result.diagnostics.front().path.empty()) {
            EXPECT_EQ(result.diagnostics.front().path, "invalid.scm");
        }
        EXPECT_GT(result.diagnostics.front().line, 0);
        EXPECT_GT(result.diagnostics.front().column, 0);
    }
}

INSTANTIATE_TEST_SUITE_P(
    SemanticErrors,
    MaterialFrontendFailure,
    testing::Values(
        "(unknown M)",
        "(material M (asset \"/Game/Generated/Materials/M\") (domain ui) (blend additive) "
        "(emissive missing))",
        "(material M (asset \"/Game/Generated/Materials/M\") (domain ui) (domain ui) "
        "(blend additive) (emissive (float3 1 1 1)))",
        "(material M (asset \"/Game/Generated/Materials/M\") (domain ui) (blend additive) "
        "(parameter scalar X 1) (parameter scalar X 2) (emissive (float3 1 1 1)))",
        "(material M (asset \"/Game/Materials/M\") (domain ui) (blend additive) "
        "(emissive (float3 1 1 1)))",
        "(material M (asset \"/Game/Generated/Materials/Other\") (domain ui) (blend additive) "
        "(emissive (float3 1 1 1)))",
        "(material M (asset \"/Game/Generated/Materials/M\") (domain ui) (blend additive) "
        "(parameter texture T \"/Missing/T\") (emissive (float3 1 1 1)))",
        "(material M (asset \"/Game/Generated/Materials/M\") (domain ui) (blend additive) "
        "(parameter texture T \"/Game/T\") (emissive (+ T 1)))",
        "(material M (asset \"/Game/Generated/Materials/M\") (domain ui) (blend additive) "
        "(emissive (sample (float4 1 1 1 1) (float2 0 0))))",
        "(material M (asset \"/Game/Generated/Materials/M\") (domain ui) (blend additive) "
        "(emissive (float2 1 1)))",
        "(material M (asset \"/Game/Generated/Materials/M\") (domain ui) (blend additive) "
        "(emissive (custom float3 ((X float 1) (X float 2)) \"return 0;\")))",
        "(material M (asset \"/Game/Generated/Materials/M\") (domain ui) (blend additive) "
        "(emissive (custom texture () \"return 0;\")))",
        "(material M (asset \"/Game/Generated/Materials/M\") (domain ui) (blend additive) "
        "(emissive (custom float3 () \"\")))",
        "(material M (asset \"/Game/Generated/Materials/M\") (domain ui) (blend additive) "
        "(emissive (float3 (sin (float2 1 2)) 1 1)))",
        "(material M (asset \"/Game/Generated/Materials/M\") (domain ui) (blend additive) "
        "(emissive (float3 (time 1) 1 1)))",
        "(material M (asset \"/Game/Generated/Materials/M\") (domain ui) (blend additive) "
        "(emissive (float3 (+ 1) 1 1)))"));

}
}
