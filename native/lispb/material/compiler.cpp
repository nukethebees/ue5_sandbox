#include <material_gen/CompiledMaterial.h>
#include <material_gen/MaterialFrontend.h>
#include <material_gen/SourceHash.h>

#include <lispb/material_compiler.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
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

auto read_file(fs::path const& path) -> std::optional<std::string> {
    std::ifstream stream{path, std::ios::binary};
    if (!stream) {
        return std::nullopt;
    }
    std::ostringstream contents;
    contents << stream.rdbuf();
    if (stream.bad()) {
        return std::nullopt;
    }
    return contents.str();
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
                                                                                 : "masked"};
    auto const shading{material.settings.shading_model == material_synth::ShadingModel::unlit
                           ? "unlit"
                           : "default-lit"};
    stream << "material " << material.settings.name << '\n'
           << "asset " << material.settings.package_path << '\n'
           << "domain " << domain << "\nblend " << blend << "\nshading " << shading << '\n'
           << "two-sided " << material.settings.two_sided << "\ndisable-depth-test "
           << material.settings.disable_depth_test << "\nused-with-instanced-static-meshes "
           << material.settings.used_with_instanced_static_meshes << "\nadopt-existing "
           << material.settings.adopt_existing << "\nopacity-mask-clip "
           << material.settings.opacity_mask_clip_value << '\n';

    stream << "parameters " << material.parameters.size() << '\n';
    for (std::size_t index{}; index < material.parameters.size(); ++index) {
        auto const& parameter{material.parameters[index]};
        stream << "  [" << index << "] " << parameter.name << ' ' << type_name(parameter.type)
               << " node=" << parameter.node.index;
        if (parameter.type == material_synth::ValueType::texture) {
            stream << " default=" << parameter.texture_path;
        } else {
            stream << " default=[";
            auto const components{material_synth::component_count(parameter.type)};
            for (std::size_t component{}; component < components; ++component) {
                stream << (component == 0 ? "" : ",") << parameter.default_value[component];
            }
            stream << ']';
        }
        stream << '\n';
    }

    stream << "nodes " << material.nodes.size() << '\n';
    for (std::size_t index{}; index < material.nodes.size(); ++index) {
        auto const& node{material.nodes[index]};
        stream << "  [" << index << "] " << kind_name(node.kind) << ' ' << type_name(node.type)
               << " inputs=";
        for (std::size_t input{}; input < node.inputs.size(); ++input) {
            stream << (input == 0 ? "[" : ",") << node.inputs[input].index;
        }
        stream << (node.inputs.empty() ? "[]" : "]");
        if (node.kind == material_synth::NodeKind::constant) {
            stream << " value=[";
            for (std::size_t component{}; component < node.component_count; ++component) {
                stream << (component == 0 ? "" : ",") << node.constant[component];
            }
            stream << ']';
        } else if (node.kind == material_synth::NodeKind::parameter) {
            stream << " parameter=" << node.parameter_index;
        } else if (node.kind == material_synth::NodeKind::texture_coordinate) {
            stream << " coordinate=" << node.coordinate_index;
        } else if (node.kind == material_synth::NodeKind::per_instance_custom_data) {
            stream << " data-index=" << node.instance_data_index;
        } else if (node.kind == material_synth::NodeKind::custom) {
            stream << " description=" << std::quoted(node.description)
                   << " code=" << std::quoted(node.code);
        } else if (node.kind == material_synth::NodeKind::texture_object) {
            stream << " texture=" << std::quoted(node.texture_path);
        } else if (node.kind == material_synth::NodeKind::component_mask) {
            stream << " mask=" << node.component_mask;
        } else if (node.kind == material_synth::NodeKind::shader_call) {
            stream << " include=" << std::quoted(node.shader_path)
                   << " function=" << node.shader_function;
        }
        stream << " @ " << node.span.path << ':' << node.span.line << ':' << node.span.column
               << '\n';
        for (auto const& custom_input : node.custom_inputs) {
            stream << "    custom-input " << custom_input.name << ' '
                   << type_name(custom_input.type) << " node=" << custom_input.node.index << '\n';
        }
    }

    for (auto const& binding : material.bindings) {
        stream << "binding " << binding.name << " node=" << binding.node.index << '\n';
    }
    for (auto const& output : material.outputs) {
        stream << "output " << output.name << " node=" << output.node.index << '\n';
    }
    for (auto const& dependency : material.texture_dependencies) {
        stream << "texture " << dependency << '\n';
    }
}

} // namespace

auto lispb::compile_material(fs::path const& input_path,
                             fs::path const& project_root_path,
                             fs::path output) -> Compilation {
    auto const input{fs::absolute(input_path).lexically_normal()};
    auto const project_root{fs::absolute(project_root_path).lexically_normal()};
    auto const source{read_file(input)};
    if (!source) {
        throw std::runtime_error{"Unable to read material source: " + input.string()};
    }

    std::error_code relative_error;
    auto const relative_input{fs::relative(input, project_root, relative_error)};
    auto const source_path{relative_error ? input.generic_string()
                                          : relative_input.generic_string()};
    ProjectTextureResolver resolver{.project_root = project_root};
    auto analysis{material_synth::analyze(source_path, *source, {&resolver, resolve_texture})};
    if (!analysis.material) {
        std::ostringstream diagnostics;
        for (auto const& diagnostic : analysis.diagnostics) {
            diagnostics << diagnostic.path << ':' << diagnostic.line << ':' << diagnostic.column
                        << ": error: " << diagnostic.message << '\n';
        }
        throw std::runtime_error{diagnostics.str()};
    }

    std::vector<fs::path> shader_dependencies;
    for (auto const& node : analysis.material->nodes) {
        if (node.kind != material_synth::NodeKind::shader_call) {
            continue;
        }
        fs::path shader_file;
        constexpr std::string_view plugin_prefix{"/Plugin/"};
        constexpr std::string_view project_prefix{"/Project/"};
        if (node.shader_path.starts_with(plugin_prefix)) {
            auto const remaining{std::string_view{node.shader_path}.substr(plugin_prefix.size())};
            auto const separator{remaining.find('/')};
            if (separator != std::string_view::npos) {
                shader_file = project_root / "Plugins" / remaining.substr(0, separator) /
                              "Shaders" / remaining.substr(separator + 1);
            }
        } else if (node.shader_path.starts_with(project_prefix)) {
            shader_file = project_root / "Shaders" /
                          std::string_view{node.shader_path}.substr(project_prefix.size());
        }
        std::error_code shader_error;
        if (shader_file.empty() || !fs::is_regular_file(shader_file, shader_error) ||
            shader_error) {
            throw std::runtime_error{"Unable to resolve material shader include: " +
                                     node.shader_path};
        }
        shader_dependencies.push_back(std::move(shader_file));
    }

    std::ostringstream debug_dump;
    dump_ir(debug_dump, *analysis.material);

    auto const source_bytes{
        std::span{reinterpret_cast<std::uint8_t const*>(source->data()), source->size()}};
    auto artifact{material_synth::serialize({.source_path = source_path,
                                             .source_hash = material_synth::sha256(source_bytes),
                                             .material = std::move(*analysis.material)})};
    if (!artifact) {
        throw std::runtime_error{"Unable to serialize compiled material: " + artifact.error()};
    }

    Compilation result{
        .artifacts = {BinaryArtifact{.path = std::move(output), .content = std::move(*artifact)}},
        .dependencies = {input},
        .debug_dump = std::move(debug_dump).str(),
    };
    result.dependencies.insert(result.dependencies.end(),
                               std::make_move_iterator(resolver.dependencies.begin()),
                               std::make_move_iterator(resolver.dependencies.end()));
    result.dependencies.insert(result.dependencies.end(),
                               std::make_move_iterator(shader_dependencies.begin()),
                               std::make_move_iterator(shader_dependencies.end()));
    return result;
}
