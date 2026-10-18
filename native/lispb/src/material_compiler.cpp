#include <lispb/material_compiler.h>

#include <ioj/format_writer.h>

#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>
#include <lispb/compilation.h>
#include <material_gen/BlendMode.h>
#include <material_gen/CompiledMaterial.h>
#include <material_gen/Diagnostic.h>
#include <material_gen/MaterialDomain.h>
#include <material_gen/MaterialFrontend.h>
#include <material_gen/MaterialIR.h>
#include <material_gen/NodeKind.h>
#include <material_gen/ShadingModel.h>
#include <material_gen/SourceHash.h>
#include <material_gen/TextureResolver.h>
#include <material_gen/TextureSamplerType.h>
#include <material_gen/ValueType.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;

struct ProjectResolver {
    fs::path project_root;
    mutable std::vector<fs::path> dependencies;
};

auto resolve_texture(void const* const context, std::string_view const requested)
    -> std::optional<std::string> {
    auto const& resolver{*static_cast<ProjectResolver const*>(context)};
    if (requested.empty() || requested.front() != '/') {
        return std::nullopt;
    }

    auto package_path{std::string{requested}};
    auto const slash{package_path.rfind('/')};
    auto const object_separator{package_path.rfind('.')};
    if (object_separator != std::string::npos && object_separator > slash) {
        package_path.resize(object_separator);
    }

    auto const mount_end{package_path.find('/', 1)};
    if (mount_end == std::string::npos || mount_end + 1 >= package_path.size()) {
        return std::nullopt;
    }
    auto const mount{package_path.substr(1, mount_end - 1)};
    auto const relative_package{package_path.substr(mount_end + 1)};
    if (mount == "Engine") {
        auto const leaf_position{package_path.rfind('/')};
        return package_path + "." + package_path.substr(leaf_position + 1);
    }
    auto asset_file{mount == "Game"
                        ? resolver.project_root / "Content" / relative_package
                        : resolver.project_root / "Plugins" / mount / "Content" / relative_package};
    asset_file += ".uasset";

    std::error_code error;
    if (!fs::is_regular_file(asset_file, error) || error) {
        return std::nullopt;
    }
    if (std::ranges::find(resolver.dependencies, asset_file) == resolver.dependencies.end()) {
        resolver.dependencies.push_back(asset_file);
    }
    auto const leaf_position{package_path.rfind('/')};
    return package_path + "." + package_path.substr(leaf_position + 1);
}

auto resolve_shader_include(ProjectResolver& resolver, std::string_view const requested) -> bool {
    fs::path shader_file;
    constexpr std::string_view plugin_prefix{"/Plugin/"};
    constexpr std::string_view project_prefix{"/Project/"};
    if (requested.starts_with(plugin_prefix)) {
        auto const remaining{requested.substr(plugin_prefix.size())};
        auto const separator{remaining.find('/')};
        if (separator != std::string_view::npos) {
            shader_file = resolver.project_root / "Plugins" / remaining.substr(0, separator) /
                          "Shaders" / remaining.substr(separator + 1);
        }
    } else if (requested.starts_with(project_prefix)) {
        shader_file = resolver.project_root / "Shaders" / requested.substr(project_prefix.size());
    }

    std::error_code error;
    if (shader_file.empty() || !fs::is_regular_file(shader_file, error) || error) {
        return false;
    }
    if (std::ranges::find(resolver.dependencies, shader_file) == resolver.dependencies.end()) {
        resolver.dependencies.push_back(std::move(shader_file));
    }
    return true;
}

auto analyze_material(lispb::codegen::sexpr::Syntax& syntax,
                      lispb::codegen::sexpr::SourceFileId const source,
                      ProjectResolver& resolver,
                      material_synth::MaterialIR& material)
    -> std::vector<material_synth::Diagnostic> {
    auto diagnostics{
        material_synth::analyze(syntax, source, {&resolver, resolve_texture}, material)};
    if (!diagnostics.empty()) {
        return diagnostics;
    }

    auto const& nodes{material.nodes};
    for (auto const node : nodes.handles()) {
        if (nodes.kind(node) != material_synth::NodeKind::shader_call) {
            continue;
        }
        auto const requested{material.text(nodes.shader_path(node))};
        if (!resolve_shader_include(resolver, requested)) {
            auto const span{nodes.span(node)};
            diagnostics.push_back({std::string{material.text(span.path)},
                                   span.line,
                                   span.column,
                                   "unresolved shader include '" + std::string{requested} + "'"});
        }
    }
    return diagnostics;
}

auto type_name(material_synth::ValueType const type) -> std::string_view {
    using enum material_synth::ValueType;
    switch (type) {
        case invalid:
            return "invalid";
        case float1:
            return "float";
        case float2:
            return "float2";
        case float3:
            return "float3";
        case float4:
            return "float4";
        case texture:
            return "texture";
    }
    return "invalid";
}

auto kind_name(material_synth::NodeKind const kind) -> std::string_view {
    using enum material_synth::NodeKind;
    switch (kind) {
        case constant:
            return "constant";
        case parameter:
            return "parameter";
        case texture_coordinate:
            return "texcoord";
        case per_instance_custom_data:
            return "per-instance-custom-data";
        case add:
            return "add";
        case subtract:
            return "subtract";
        case multiply:
            return "multiply";
        case divide:
            return "divide";
        case lerp:
            return "lerp";
        case saturate:
            return "saturate";
        case sample:
            return "sample";
        case custom:
            return "custom";
        case vector_constructor:
            return "vector";
        case time:
            return "time";
        case sine:
            return "sine";
        case cosine:
            return "cosine";
        case texture_object:
            return "texture";
        case component_mask:
            return "swizzle";
        case world_position:
            return "world-position";
        case object_position:
            return "object-position";
        case pixel_normal:
            return "pixel-normal";
        case vertex_normal:
            return "vertex-normal";
        case camera_vector:
            return "camera-vector";
        case camera_position:
            return "camera-position";
        case step:
            return "step";
        case vertex_color:
            return "vertex-color";
        case transform_position:
            return "transform-position";
        case scene_texture:
            return "scene-texture";
        case shader_call:
            return "shader-call";
    }
    return "unknown";
}

auto dump_ir(material_synth::MaterialIR const& material) -> std::string {
    std::string result;
    ioj::FormatWriter write{std::back_inserter(result)};
    auto const& settings{material.settings};
    auto const& parameters{material.parameters};
    auto const& nodes{material.nodes};
    auto const& bindings{material.bindings};
    auto const& outputs{material.outputs};
    auto quoted = [&result](std::string_view const text) {
        result += '"';
        for (auto const character : text) {
            if (character == '"' || character == '\\') {
                result += '\\';
            }
            result += character;
        }
        result += '"';
    };

    auto const domain{settings.domain == material_synth::MaterialDomain::ui ? "ui"
                      : settings.domain == material_synth::MaterialDomain::surface
                          ? "surface"
                          : "post-process"};
    auto const blend{settings.blend_mode == material_synth::BlendMode::additive      ? "additive"
                     : settings.blend_mode == material_synth::BlendMode::translucent ? "translucent"
                     : settings.blend_mode == material_synth::BlendMode::opaque      ? "opaque"
                     : settings.blend_mode == material_synth::BlendMode::masked
                         ? "masked"
                         : "alpha-composite"};
    auto const shading{
        settings.shading_model == material_synth::ShadingModel::unlit ? "unlit" : "default-lit"};
    write("material {}\nasset {}\ndomain {}\nblend {}\nshading {}\n"
          "two-sided {:d}\ndisable-depth-test {:d}\nused-with-instanced-static-meshes {:d}\n"
          "used-with-particle-sprites {:d}\nadopt-existing {:d}\nopacity-mask-clip {:.6g}\n",
          material.text(settings.name),
          material.text(settings.package_path),
          domain,
          blend,
          shading,
          settings.two_sided,
          settings.disable_depth_test,
          settings.used_with_instanced_static_meshes,
          settings.used_with_particle_sprites,
          settings.adopt_existing,
          settings.opacity_mask_clip_value);

    write("parameters {}\n", parameters.size());
    for (auto const parameter : parameters.handles()) {
        auto const index{parameter.value};
        auto const type{parameters.type[index]};
        write("  [{}] {} {} node={}",
              index,
              material.text(parameters.name[index]),
              type_name(type),
              parameters.node[index].value);
        if (type == material_synth::ValueType::texture) {
            auto const sampler{parameters.texture_sampler_type[index] ==
                                       material_synth::TextureSamplerType::linear_grayscale
                                   ? "linear-grayscale"
                                   : "linear-color"};
            write(" default={} sampler={}", material.text(parameters.texture_path[index]), sampler);
        } else {
            result += " default=[";
            auto const components{material_synth::component_count(type)};
            for (std::size_t component{}; component < components; ++component) {
                write("{}{:.6g}",
                      component == 0 ? "" : ",",
                      parameters.default_values[component][index]);
            }
            result += ']';
        }
        result += '\n';
    }

    write("nodes {}\n", nodes.size());
    for (auto const node : nodes.handles()) {
        auto const index{node.value};
        auto const kind{nodes.kind(node)};
        auto const inputs{nodes.inputs(node)};
        auto const input_count{inputs.size()};
        write("  [{}] {} {} inputs=", index, kind_name(kind), type_name(nodes.type(node)));
        for (std::size_t input{}; input < input_count; ++input) {
            write("{}{}", input == 0 ? "[" : ",", inputs[input].value);
        }
        result += inputs.empty() ? "[]" : "]";
        if (kind == material_synth::NodeKind::constant) {
            result += " value=[";
            auto const component_count{nodes.component_count(node)};
            for (std::size_t component{}; component < component_count; ++component) {
                write("{}{:.6g}", component == 0 ? "" : ",", nodes.constant(node, component));
            }
            result += ']';
        } else if (kind == material_synth::NodeKind::parameter) {
            write(" parameter={}", nodes.parameter(node).value);
        } else if (kind == material_synth::NodeKind::texture_coordinate) {
            write(" coordinate={}", nodes.coordinate_index(node));
        } else if (kind == material_synth::NodeKind::per_instance_custom_data) {
            write(" data-index={}", nodes.instance_data_index(node));
        } else if (kind == material_synth::NodeKind::custom) {
            result += " description=";
            quoted(material.text(nodes.description(node)));
            result += " code=";
            quoted(material.text(nodes.code(node)));
        } else if (kind == material_synth::NodeKind::texture_object) {
            result += " texture=";
            quoted(material.text(nodes.texture_path(node)));
        } else if (kind == material_synth::NodeKind::component_mask) {
            write(" mask={}", material.text(nodes.component_mask(node)));
        } else if (kind == material_synth::NodeKind::shader_call) {
            result += " include=";
            quoted(material.text(nodes.shader_path(node)));
            write(" function={}", material.text(nodes.shader_function(node)));
        }
        auto const span{nodes.span(node)};
        write(" @ {}:{}:{}\n", material.text(span.path), span.line, span.column);
        if (kind == material_synth::NodeKind::custom ||
            kind == material_synth::NodeKind::shader_call) {
            auto const custom_inputs{nodes.custom_inputs(node)};
            auto const custom_input_count{custom_inputs.size()};
            for (std::size_t input{}; input < custom_input_count; ++input) {
                auto const custom_input{custom_inputs[input]};
                write("    custom-input {} {} node={}\n",
                      material.text(nodes.input_declarations.name[custom_input.value]),
                      type_name(nodes.input_declarations.type[custom_input.value]),
                      inputs[input].value);
            }
        }
    }

    for (auto const binding : bindings.handles()) {
        write("binding {} node={}\n",
              material.text(bindings.name[binding.value]),
              bindings.node[binding.value].value);
    }
    for (auto const output : outputs.handles()) {
        write("output {} node={}\n",
              material.text(outputs.name[output.value]),
              outputs.node[output.value].value);
    }
    for (auto const& dependency : material.texture_dependencies) {
        write("texture {}\n", material.text(dependency));
    }
    return result;
}

} // namespace

void lispb::compile_material(lispb::codegen::sexpr::Syntax& syntax,
                             lispb::codegen::sexpr::SourceFileId const source,
                             material_synth::MaterialIR& material,
                             Compilation& result,
                             fs::path const& input_path,
                             fs::path const& project_root_path,
                             fs::path output,
                             bool const emit) {
    auto const input{fs::absolute(input_path).lexically_normal()};
    auto const project_root{fs::absolute(project_root_path).lexically_normal()};
    ProjectResolver resolver{.project_root = project_root};
    auto const analysis{analyze_material(syntax, source, resolver, material)};
    if (!analysis.empty()) {
        std::string diagnostics;
        ioj::FormatWriter write{std::back_inserter(diagnostics)};
        for (auto const& diagnostic : analysis) {
            write("{}:{}:{}: error: {}\n",
                  diagnostic.path,
                  diagnostic.line,
                  diagnostic.column,
                  diagnostic.message);
        }
        throw std::runtime_error{diagnostics};
    }

    result.dependencies.push_back(input);
    result.dependencies.insert(
        result.dependencies.end(), resolver.dependencies.begin(), resolver.dependencies.end());
    if (!emit) {
        return;
    }

    result.debug_dump = dump_ir(material);

    auto const source_text{syntax.sources().text(source)};
    auto const source_bytes{
        std::span{reinterpret_cast<std::uint8_t const*>(source_text.data()), source_text.size()}};
    auto artifact{material_synth::serialize(
        material, syntax.sources().path(source), material_synth::sha256(source_bytes))};
    if (!artifact) {
        throw std::runtime_error{"Unable to serialize compiled material: " + artifact.error()};
    }
    result.add_binary(std::move(output), std::move(*artifact));
}
