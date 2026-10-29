#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>
#include <lispb/diagnostics.h>
#include <lispb/project_loader.h>
#include <lispb/strings.h>
#include <material_gen/BlendMode.h>
#include <material_gen/CompiledMaterial.h>
#include <material_gen/Diagnostic.h>
#include <material_gen/Indices.h>
#include <material_gen/MaterialDomain.h>
#include <material_gen/MaterialFrontend.h>
#include <material_gen/MaterialIR.h>
#include <material_gen/NamedNodes.h>
#include <material_gen/NodeKind.h>
#include <material_gen/Nodes.h>
#include <material_gen/Parameters.h>
#include <material_gen/ShadingModel.h>
#include <material_gen/SourceHash.h>
#include <material_gen/TextureResolver.h>
#include <material_gen/TextureSamplerType.h>
#include <material_gen/ValueType.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace material_synth {
namespace {

struct AnalyzedMaterial {
    std::unique_ptr<lispb::Strings> strings;
    std::optional<MaterialIR> material;
    std::vector<Diagnostic> diagnostics;
};

auto analyze(std::string_view path, std::string_view source, TextureResolver resolver)
    -> AnalyzedMaterial {
    auto strings{std::make_unique<lispb::Strings>()};
    lispb::Diagnostics parser_diagnostics{*strings};
    lispb::codegen::sexpr::Sources sources{*strings, parser_diagnostics};
    lispb::codegen::sexpr::Syntax syntax{*strings, sources};
    auto const file{sources.add(path, source)};
    MaterialIR material{*strings};
    auto diagnostics{material_synth::analyze(syntax, file, resolver, material)};
    if (!diagnostics.empty()) {
        return {std::move(strings), {}, std::move(diagnostics)};
    }
    return {std::move(strings), std::move(material), {}};
}

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
    return std::string{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

auto node_count(MaterialIR const& material, NodeKind const kind) -> std::ptrdiff_t {
    return std::ranges::count_if(material.nodes.handles(), [&](NodeHandle const node) {
        return material.nodes.kind(node) == kind;
    });
}

TEST(MaterialFrontend, LowersUiGlowGoldenSourceWithStableHandles) {
    auto const source_path{std::filesystem::path{IOJ_PROJECT_SOURCE_DIR} /
                           "Plugins/SandboxUI/Source/SandboxUI/Private/materials/"
                           "UiGlowComposite.lispb"};
    auto const result{analyze(
        source_path.generic_string(), read(source_path), TextureResolver{nullptr, resolve})};
    ASSERT_TRUE(result.material.has_value())
        << (result.diagnostics.empty() ? "" : result.diagnostics.front().message);
    auto const& material{*result.material};
    auto const node_handles{material.nodes.handles()};
    EXPECT_EQ(material.text(material.settings.name), "M_UiGlowComposite");
    EXPECT_EQ(material.text(material.settings.package_path),
              "/SandboxUI/Generated/Materials/M_UiGlowComposite");
    ASSERT_EQ(material.parameters.size(), 4);
    for (std::size_t index{}; index < material.parameters.size(); ++index) {
        EXPECT_EQ(material.parameters.node[index].value, index);
    }
    EXPECT_EQ(material.nodes.kind(NodeHandle{4}), NodeKind::texture_coordinate);
    EXPECT_EQ(node_count(material, NodeKind::sample), 2);
    auto const custom{std::ranges::find_if(node_handles, [&](NodeHandle node) {
        return material.nodes.kind(node) == NodeKind::custom;
    })};
    ASSERT_NE(custom, node_handles.end());
    EXPECT_EQ(material.nodes.type(*custom), ValueType::float1);
    ASSERT_EQ(material.nodes.custom_inputs(*custom).size(), 2);
    EXPECT_EQ(
        material.text(
            material.nodes.input_declarations.name[material.nodes.custom_inputs(*custom)[1].value]),
        "CoreAlpha");
    EXPECT_EQ(material.nodes.kind(material.outputs.node.front()), NodeKind::multiply);
    EXPECT_EQ(material.texture_dependencies.size(), 1);
    EXPECT_TRUE(validate(material).empty());
}

TEST(MaterialFrontend, LowersVertexColourForProceduralRendererMaterials) {
    auto analysis{analyze("vertex-colour.scm",
                          "(material M (asset \"/Game/Generated/Materials/M\") (domain surface) "
                          "(blend additive) (shading unlit) "
                          "(usage particle-sprites) "
                          "(emissive (swizzle (vertex-color) rgb)))",
                          TextureResolver{nullptr, resolve})};
    ASSERT_TRUE(analysis.material.has_value());
    auto const& material{*analysis.material};
    auto const node_handles{material.nodes.handles()};
    auto const vertex_colour{std::ranges::find_if(node_handles, [&](NodeHandle node) {
        return material.nodes.kind(node) == NodeKind::vertex_color;
    })};
    ASSERT_NE(vertex_colour, node_handles.end());
    EXPECT_EQ(material.nodes.type(*vertex_colour), ValueType::float4);
    EXPECT_TRUE(material.settings.used_with_particle_sprites);
    EXPECT_TRUE(validate(material).empty());
}

TEST(CompiledMaterial, RoundTripsDeterministically) {
    auto const source_path{std::filesystem::path{IOJ_PROJECT_SOURCE_DIR} /
                           "Plugins/SandboxUI/Source/SandboxUI/Private/materials/"
                           "UiGlowComposite.lispb"};
    auto const source{read(source_path)};
    auto analysis{analyze(source_path.generic_string(), source, TextureResolver{nullptr, resolve})};
    ASSERT_TRUE(analysis.material.has_value());
    auto const source_bytes{
        std::span{reinterpret_cast<std::uint8_t const*>(source.data()), source.size()}};
    std::string_view const relative_path{
        "Plugins/SandboxUI/Source/SandboxUI/Private/materials/UiGlowComposite.lispb"};
    auto const source_hash{sha256(source_bytes)};
    auto const& material{*analysis.material};

    auto const first{serialize(material, relative_path, source_hash)};
    auto const second{serialize(material, relative_path, source_hash)};
    ASSERT_TRUE(first.has_value()) << first.error();
    ASSERT_TRUE(second.has_value()) << second.error();
    EXPECT_EQ(*first, *second);

    auto const decoded{deserialize(*first)};
    ASSERT_TRUE(decoded.has_value()) << decoded.error();
    EXPECT_EQ(decoded->material.text(decoded->source_path), relative_path);
    EXPECT_EQ(decoded->material.text(decoded->source_hash), source_hash);
    EXPECT_EQ(decoded->material.text(decoded->material.settings.name), "M_UiGlowComposite");
    EXPECT_EQ(decoded->material.parameters.size(), 4);
    EXPECT_GT(node_count(decoded->material, NodeKind::component_mask), 0);
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
    auto artifact{serialize(*analysis.material,
                            "source.scm",
                            "0000000000000000000000000000000000000000000000000000000000000000")};
    ASSERT_TRUE(artifact.has_value());
    artifact->push_back(0);
    EXPECT_FALSE(deserialize(*artifact).has_value());
}

TEST(MaterialFrontend, LowersWorldSurfaceSettingsInstanceDataAndOpacity) {
    auto const source_path{std::filesystem::path{IOJ_PROJECT_SOURCE_DIR} /
                           "Plugins/SpaceGame/Source/SpaceGamePresentation/Private/materials/"
                           "SoftTargetWorld.lispb"};
    auto const result{analyze(
        source_path.generic_string(), read(source_path), TextureResolver{nullptr, resolve})};
    ASSERT_TRUE(result.material.has_value())
        << (result.diagnostics.empty() ? "" : result.diagnostics.front().message);
    auto const& material{*result.material};
    auto const node_handles{material.nodes.handles()};
    EXPECT_EQ(material.settings.domain, MaterialDomain::surface);
    EXPECT_EQ(material.settings.blend_mode, BlendMode::translucent);
    EXPECT_EQ(material.settings.shading_model, ShadingModel::unlit);
    EXPECT_TRUE(material.settings.two_sided);
    EXPECT_TRUE(material.settings.disable_depth_test);
    EXPECT_TRUE(material.settings.used_with_instanced_static_meshes);
    ASSERT_EQ(material.parameters.size(), 16);
    auto const parameter_ids{material.parameters.handles()};
    auto const filaments{std::ranges::find_if(parameter_ids, [&](ParameterId parameter) {
        return material.text(material.parameters.name[parameter.value]) == "EnergyFilaments";
    })};
    ASSERT_NE(filaments, parameter_ids.end());
    EXPECT_EQ(material.parameters.texture_sampler_type[(*filaments).value],
              TextureSamplerType::linear_grayscale);
    auto const flow{std::ranges::find_if(parameter_ids, [&](ParameterId parameter) {
        return material.text(material.parameters.name[parameter.value]) == "EnergyFlow";
    })};
    ASSERT_NE(flow, parameter_ids.end());
    EXPECT_EQ(material.parameters.texture_sampler_type[(*flow).value],
              TextureSamplerType::linear_color);
    for (unsigned instance_data_index{}; instance_data_index < 6; ++instance_data_index) {
        EXPECT_TRUE(std::ranges::any_of(node_handles, [&](NodeHandle node) {
            return material.nodes.kind(node) == NodeKind::per_instance_custom_data &&
                   material.nodes.instance_data_index(node) == instance_data_index;
        }));
    }
    EXPECT_GT(node_count(material, NodeKind::time), 0);
    EXPECT_GT(node_count(material, NodeKind::saturate), 0);
    EXPECT_GT(node_count(material, NodeKind::lerp), 0);
    EXPECT_EQ(node_count(material, NodeKind::custom), 2);
    ASSERT_EQ(material.outputs.size(), 2);
    EXPECT_EQ(material.text(material.outputs.name[1]), "opacity");
    EXPECT_TRUE(validate(material).empty());

    auto const bytes{serialize(material,
                               source_path.generic_string(),
                               "0000000000000000000000000000000000000000000000000000000000000000")};
    ASSERT_TRUE(bytes.has_value()) << bytes.error();
    auto const decoded{deserialize(*bytes)};
    ASSERT_TRUE(decoded.has_value()) << decoded.error();
    EXPECT_EQ(decoded->material.settings.domain, MaterialDomain::surface);
    EXPECT_EQ(decoded->material.settings.shading_model, ShadingModel::unlit);
    EXPECT_TRUE(decoded->material.settings.used_with_instanced_static_meshes);
    EXPECT_EQ(decoded->material.parameters.size(), 16);
    EXPECT_GT(node_count(decoded->material, NodeKind::time), 0);
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

    auto const binding_ids{material.bindings.handles()};
    auto const find_binding{[&](std::string_view const name) -> NodeHandle {
        auto const binding{std::ranges::find_if(binding_ids, [&](NamedNodeId candidate) {
            return material.text(material.bindings.name[candidate.value]) == name;
        })};
        EXPECT_NE(binding, binding_ids.end());
        return material.bindings.node[(*binding).value];
    }};

    EXPECT_EQ(material.nodes.kind(find_binding("phase")), NodeKind::multiply);
    EXPECT_EQ(material.nodes.kind(find_binding("pulse")), NodeKind::multiply);
    EXPECT_EQ(material.nodes.kind(find_binding("offset")), NodeKind::cosine);
    EXPECT_EQ(material.nodes.kind(find_binding("tint")), NodeKind::vector_constructor);
    EXPECT_GT(node_count(material, NodeKind::time), 0);
    EXPECT_GT(node_count(material, NodeKind::sine), 0);
    auto const emissive{material.outputs.node.front()};
    EXPECT_EQ(material.nodes.kind(emissive), NodeKind::multiply);
    EXPECT_EQ(material.nodes.inputs(emissive).size(), 4);
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
    EXPECT_EQ(material.nodes.type(NodeHandle{2}), ValueType::float2);
    EXPECT_EQ(material.nodes.type(NodeHandle{3}), ValueType::float3);
    EXPECT_EQ(material.nodes.type(NodeHandle{4}), ValueType::float4);
    auto const binding_ids{material.bindings.handles()};
    auto const sampled{std::ranges::find_if(binding_ids, [&](NamedNodeId binding) {
        return material.text(material.bindings.name[binding.value]) == "sampled";
    })};
    ASSERT_NE(sampled, binding_ids.end());
    EXPECT_EQ(material.nodes.kind(material.bindings.node[(*sampled).value]), NodeKind::sample);
    EXPECT_EQ(material.nodes.type(material.bindings.node[(*sampled).value]), ValueType::float4);
}

TEST(MaterialFrontend, LowersMigratedMaterialsToNativeScaffoldingAndTypedShaderLeaves) {
    auto const source_root{std::filesystem::path{IOJ_PROJECT_SOURCE_DIR}};
    std::array<std::filesystem::path, 9> const sources{
        source_root / "Plugins/SandboxExperiments/Source/SbxShadersExperiments/Private/materials/"
                      "RadarDisplay.lispb",
        source_root / "Plugins/SandboxExperiments/Source/SbxShadersExperiments/Private/materials/"
                      "EnergyShield.lispb",
        source_root / "Plugins/SandboxExperiments/Source/SbxShadersExperiments/Private/materials/"
                      "SpaceEnergyFieldDisplay.lispb",
        source_root / "Source/Sandbox/materials/GlowingCross.lispb",
        source_root / "Plugins/SandboxExperiments/Source/SbxShadersExperiments/Private/materials/"
                      "VertexRipple.lispb",
        source_root / "Plugins/SandboxExperiments/Source/SbxShadersExperiments/Private/materials/"
                      "ConstructionSpawn.lispb",
        source_root / "Plugins/SandboxExperiments/Source/SbxShadersExperiments/Private/materials/"
                      "PlanetAtmosphere.lispb",
        source_root / "Plugins/SandboxExperiments/Source/SbxShadersExperiments/Private/materials/"
                      "PlanetSurface.lispb",
        source_root / "Plugins/SandboxExperiments/Source/SbxShadersExperiments/Private/materials/"
                      "TacticalScan.lispb"};

    std::vector<AnalyzedMaterial> materials;
    for (auto const& source_path : sources) {
        auto result{analyze(
            source_path.generic_string(), read(source_path), TextureResolver{nullptr, resolve})};
        ASSERT_TRUE(result.material.has_value())
            << source_path << ": "
            << (result.diagnostics.empty() ? "" : result.diagnostics.front().message);
        EXPECT_TRUE(result.material->settings.adopt_existing);
        EXPECT_TRUE(validate(*result.material).empty());
        materials.push_back(std::move(result));
    }

    auto const& energy_shield{*materials[1].material};
    EXPECT_GT(node_count(energy_shield, NodeKind::subtract), 0);
    EXPECT_TRUE(std::ranges::any_of(energy_shield.nodes.handles(), [&](NodeHandle node) {
        return energy_shield.nodes.kind(node) == NodeKind::shader_call &&
               energy_shield.text(energy_shield.nodes.shader_function(node)) == "sbx_energy_shield";
    }));

    auto const& glowing_cross{*materials[3].material};
    EXPECT_EQ(node_count(glowing_cross, NodeKind::custom), 0);
    EXPECT_EQ(node_count(glowing_cross, NodeKind::shader_call), 0);
    EXPECT_GT(node_count(glowing_cross, NodeKind::texture_object), 0);

    auto const& construction_spawn{*materials[5].material};
    EXPECT_EQ(construction_spawn.settings.blend_mode, BlendMode::masked);
    EXPECT_GT(node_count(construction_spawn, NodeKind::transform_position), 0);
    EXPECT_TRUE(std::ranges::any_of(construction_spawn.outputs.handles(), [&](NamedNodeId output) {
        return construction_spawn.text(construction_spawn.outputs.name[output.value]) ==
               "opacity-mask";
    }));

    auto const& tactical_scan{*materials[8].material};
    EXPECT_EQ(tactical_scan.settings.domain, MaterialDomain::post_process);
    EXPECT_EQ(node_count(tactical_scan, NodeKind::scene_texture), 2);
}

TEST(MaterialIR, RejectsForgedForwardHandlesAndMalformedNodes) {
    lispb::Strings strings;
    MaterialIR material{strings};
    material.settings.name = material.intern("M_Invalid");
    material.settings.package_path = material.intern("/Game/Generated/Materials/M_Invalid");
    material.nodes.add(
        NodeKind::add, ValueType::float1, {}, std::array{NodeHandle{1}, NodeHandle{1}});
    material.outputs.append(material.intern("emissive"), {0}, {});
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
