#include <lispb/material_compiler.h>

#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>
#include <lispb/compilation.h>
#include <material_gen/BlendMode.h>
#include <material_gen/CompiledMaterial.h>
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
#include <iomanip>
#include <optional>
#include <ostream>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;

struct ProjectTextureResolver {
    fs::path project_root;
    mutable std::vector<fs::path> dependencies;
};

auto resolve_texture(void const* const context, std::string_view const requested)
    -> std::optional<std::string> {
    auto const& resolver{*static_cast<ProjectTextureResolver const*>(context)};
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

void dump_ir(std::ostream& stream, material_synth::MaterialIR const& material) {
    auto const domain{material.settings.domain == material_synth::MaterialDomain::ui ? "ui"
                      : material.settings.domain == material_synth::MaterialDomain::surface
                          ? "surface"
                          : "post-process"};
    auto const blend{
        material.settings.blend_mode == material_synth::BlendMode::additive      ? "additive"
        : material.settings.blend_mode == material_synth::BlendMode::translucent ? "translucent"
        : material.settings.blend_mode == material_synth::BlendMode::opaque      ? "opaque"
        : material.settings.blend_mode == material_synth::BlendMode::masked ? "masked"
                                                                            : "alpha-composite"};
    auto const shading{material.settings.shading_model == material_synth::ShadingModel::unlit
                           ? "unlit"
                           : "default-lit"};
    stream << "material " << material.text(material.settings.name) << '\n'
           << "asset " << material.text(material.settings.package_path) << '\n'
           << "domain " << domain << "\nblend " << blend << "\nshading " << shading << '\n'
           << "two-sided " << material.settings.two_sided << "\ndisable-depth-test "
           << material.settings.disable_depth_test << "\nused-with-instanced-static-meshes "
           << material.settings.used_with_instanced_static_meshes << "\nused-with-particle-sprites "
           << material.settings.used_with_particle_sprites << "\nadopt-existing "
           << material.settings.adopt_existing << "\nopacity-mask-clip "
           << material.settings.opacity_mask_clip_value << '\n';

    stream << "parameters " << material.parameters.size() << '\n';
    for (auto const parameter : material.parameters.handles()) {
        auto const index{parameter.value};
        stream << "  [" << index << "] " << material.text(material.parameters.name[parameter.value])
               << ' ' << type_name(material.parameters.type[parameter.value])
               << " node=" << material.parameters.node[parameter.value].value;
        if (material.parameters.type[parameter.value] == material_synth::ValueType::texture) {
            auto const sampler{material.parameters.texture_sampler_type[parameter.value] ==
                                       material_synth::TextureSamplerType::linear_grayscale
                                   ? "linear-grayscale"
                                   : "linear-color"};
            stream << " default="
                   << material.text(material.parameters.texture_path[parameter.value])
                   << " sampler=" << sampler;
        } else {
            stream << " default=[";
            auto const components{
                material_synth::component_count(material.parameters.type[parameter.value])};
            for (std::size_t component{}; component < components; ++component) {
                stream << (component == 0 ? "" : ",")
                       << material.parameters.default_values[component][parameter.value];
            }
            stream << ']';
        }
        stream << '\n';
    }

    stream << "nodes " << material.nodes.size() << '\n';
    for (auto const node : material.nodes.handles()) {
        auto const index{node.value};
        stream << "  [" << index << "] " << kind_name(material.nodes.kind(node)) << ' '
               << type_name(material.nodes.type(node)) << " inputs=";
        for (std::size_t input{}; input < material.nodes.inputs(node).size(); ++input) {
            stream << (input == 0 ? "[" : ",") << material.nodes.inputs(node)[input].value;
        }
        stream << (material.nodes.inputs(node).empty() ? "[]" : "]");
        if (material.nodes.kind(node) == material_synth::NodeKind::constant) {
            stream << " value=[";
            for (std::size_t component{}; component < material.nodes.component_count(node);
                 ++component) {
                stream << (component == 0 ? "" : ",") << material.nodes.constant(node, component);
            }
            stream << ']';
        } else if (material.nodes.kind(node) == material_synth::NodeKind::parameter) {
            stream << " parameter=" << material.nodes.parameter(node).value;
        } else if (material.nodes.kind(node) == material_synth::NodeKind::texture_coordinate) {
            stream << " coordinate=" << material.nodes.coordinate_index(node);
        } else if (material.nodes.kind(node) ==
                   material_synth::NodeKind::per_instance_custom_data) {
            stream << " data-index=" << material.nodes.instance_data_index(node);
        } else if (material.nodes.kind(node) == material_synth::NodeKind::custom) {
            stream << " description="
                   << std::quoted(material.text(material.nodes.description(node)))
                   << " code=" << std::quoted(material.text(material.nodes.code(node)));
        } else if (material.nodes.kind(node) == material_synth::NodeKind::texture_object) {
            stream << " texture=" << std::quoted(material.text(material.nodes.texture_path(node)));
        } else if (material.nodes.kind(node) == material_synth::NodeKind::component_mask) {
            stream << " mask=" << material.text(material.nodes.component_mask(node));
        } else if (material.nodes.kind(node) == material_synth::NodeKind::shader_call) {
            stream << " include=" << std::quoted(material.text(material.nodes.shader_path(node)))
                   << " function=" << material.text(material.nodes.shader_function(node));
        }
        stream << " @ " << material.text(material.nodes.span(node).path) << ':'
               << material.nodes.span(node).line << ':' << material.nodes.span(node).column << '\n';
        if (material.nodes.kind(node) == material_synth::NodeKind::custom ||
            material.nodes.kind(node) == material_synth::NodeKind::shader_call) {
            for (std::size_t input{}; input < material.nodes.custom_inputs(node).size(); ++input) {
                auto const& custom_input{material.nodes.custom_inputs(node)[input]};
                stream << "    custom-input "
                       << material.text(material.nodes.input_declarations.name[custom_input.value])
                       << ' '
                       << type_name(material.nodes.input_declarations.type[custom_input.value])
                       << " node=" << material.nodes.inputs(node)[input].value << '\n';
            }
        }
    }

    for (auto const binding : material.bindings.handles()) {
        stream << "binding " << material.text(material.bindings.name[binding.value])
               << " node=" << material.bindings.node[binding.value].value << '\n';
    }
    for (auto const output : material.outputs.handles()) {
        stream << "output " << material.text(material.outputs.name[output.value])
               << " node=" << material.outputs.node[output.value].value << '\n';
    }
    for (auto const& dependency : material.texture_dependencies) {
        stream << "texture " << material.text(dependency) << '\n';
    }
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
    ProjectTextureResolver resolver{.project_root = project_root};
    auto const analysis{
        material_synth::analyze(syntax, source, {&resolver, resolve_texture}, material)};
    if (!analysis.empty()) {
        std::ostringstream diagnostics;
        for (auto const& diagnostic : analysis) {
            diagnostics << diagnostic.path << ':' << diagnostic.line << ':' << diagnostic.column
                        << ": error: " << diagnostic.message << '\n';
        }
        throw std::runtime_error{diagnostics.str()};
    }

    std::vector<fs::path> shader_dependencies;
    for (auto const node : material.nodes.handles()) {
        if (material.nodes.kind(node) != material_synth::NodeKind::shader_call) {
            continue;
        }
        fs::path shader_file;
        constexpr std::string_view plugin_prefix{"/Plugin/"};
        constexpr std::string_view project_prefix{"/Project/"};
        if (material.text(material.nodes.shader_path(node)).starts_with(plugin_prefix)) {
            auto const remaining{
                material.text(material.nodes.shader_path(node)).substr(plugin_prefix.size())};
            auto const separator{remaining.find('/')};
            if (separator != std::string_view::npos) {
                shader_file = project_root / "Plugins" / remaining.substr(0, separator) /
                              "Shaders" / remaining.substr(separator + 1);
            }
        } else if (material.text(material.nodes.shader_path(node)).starts_with(project_prefix)) {
            shader_file =
                project_root / "Shaders" /
                material.text(material.nodes.shader_path(node)).substr(project_prefix.size());
        }
        std::error_code shader_error;
        if (shader_file.empty() || !fs::is_regular_file(shader_file, shader_error) ||
            shader_error) {
            throw std::runtime_error{"Unable to resolve material shader include: " +
                                     std::string{material.text(material.nodes.shader_path(node))}};
        }
        shader_dependencies.push_back(std::move(shader_file));
    }

    result.dependencies.push_back(input);
    result.dependencies.insert(
        result.dependencies.end(), resolver.dependencies.begin(), resolver.dependencies.end());
    result.dependencies.insert(
        result.dependencies.end(), shader_dependencies.begin(), shader_dependencies.end());
    if (!emit) {
        return;
    }

    std::ostringstream debug_dump;
    dump_ir(debug_dump, material);
    result.debug_dump = std::move(debug_dump).str();

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
