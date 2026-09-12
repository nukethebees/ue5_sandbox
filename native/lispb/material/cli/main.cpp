#include <material_gen/CompiledMaterial.h>
#include <material_gen/MaterialFrontend.h>
#include <material_gen/SourceHash.h>

#include "material_cli.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace fs = std::filesystem;

struct Options {
    std::string_view command;
    fs::path input;
    std::optional<fs::path> output;
    std::optional<fs::path> depfile;
    std::optional<fs::path> project_root;
};

void print_usage(std::ostream& stream) {
    stream << "usage: lispb <validate|dump-ir> --input <file> "
              "[--project-root <directory>]\n"
              "       lispb compile --input <file> --output <file> "
              "[--depfile <file>] [--project-root <directory>]\n";
}

auto parse_options(int const argc, char const* const* const argv) -> std::optional<Options> {
    if (argc < 2) {
        return std::nullopt;
    }

    Options options{.command = argv[1]};
    for (int index{2}; index < argc; ++index) {
        std::string_view const argument{argv[index]};
        if (argument == "--input" && index + 1 < argc) {
            options.input = argv[++index];
        } else if (argument == "--output" && index + 1 < argc) {
            options.output = fs::path{argv[++index]};
        } else if (argument == "--depfile" && index + 1 < argc) {
            options.depfile = fs::path{argv[++index]};
        } else if (argument == "--project-root" && index + 1 < argc) {
            options.project_root = fs::path{argv[++index]};
        } else {
            return std::nullopt;
        }
    }
    if ((options.command != "validate" && options.command != "dump-ir" &&
         options.command != "compile") ||
        options.input.empty() || (options.command == "compile") != options.output.has_value()) {
        return std::nullopt;
    }
    if (options.command != "compile" && options.depfile) {
        return std::nullopt;
    }
    return options;
}

auto contains_uproject(fs::path const& directory) -> bool {
    std::error_code error;
    fs::directory_iterator iterator{directory, error};
    fs::directory_iterator const end;
    while (!error && iterator != end) {
        if (iterator->is_regular_file(error) && iterator->path().extension() == ".uproject") {
            return true;
        }
        iterator.increment(error);
    }
    return false;
}

auto find_project_root(fs::path directory) -> std::optional<fs::path> {
    std::error_code error;
    directory = fs::absolute(directory, error);
    if (error) {
        return std::nullopt;
    }
    while (!directory.empty()) {
        if (contains_uproject(directory)) {
            return directory;
        }
        auto const parent{directory.parent_path()};
        if (parent == directory) {
            break;
        }
        directory = parent;
    }
    return std::nullopt;
}

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

auto write_file(fs::path const& path, std::span<std::uint8_t const> const contents) -> bool {
    std::ofstream stream{path, std::ios::binary | std::ios::trunc};
    stream.write(reinterpret_cast<char const*>(contents.data()),
                 static_cast<std::streamsize>(contents.size()));
    return static_cast<bool>(stream);
}

auto depfile_path(fs::path const& path) -> std::string {
    auto const source{path.generic_string()};
    std::string escaped;
    escaped.reserve(source.size());
    for (char const value : source) {
        if (value == ' ' || value == '#') {
            escaped.push_back('\\');
        } else if (value == '$') {
            escaped.push_back('$');
        }
        escaped.push_back(value);
    }
    return escaped;
}

auto write_depfile(fs::path const& path,
                   fs::path const& output,
                   fs::path const& input,
                   std::span<fs::path const> const dependencies) -> bool {
    std::ofstream stream{path, std::ios::binary | std::ios::trunc};
    stream << depfile_path(output) << ": " << depfile_path(input);
    for (auto const& dependency : dependencies) {
        stream << ' ' << depfile_path(dependency);
    }
    stream << '\n';
    return static_cast<bool>(stream);
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
    }
    return "unknown";
}

void dump_ir(material_synth::MaterialIR const& material) {
    auto const domain{material.settings.domain == material_synth::MaterialDomain::ui ? "ui"
                                                                                     : "surface"};
    auto const blend{material.settings.blend_mode == material_synth::BlendMode::additive
                         ? "additive"
                         : "translucent"};
    auto const shading{material.settings.shading_model == material_synth::ShadingModel::unlit
                           ? "unlit"
                           : "default-lit"};
    std::cout << "material " << material.settings.name << '\n'
              << "asset " << material.settings.package_path << '\n'
              << "domain " << domain << "\nblend " << blend << "\nshading " << shading << '\n'
              << "two-sided " << material.settings.two_sided << "\ndisable-depth-test "
              << material.settings.disable_depth_test << "\nused-with-instanced-static-meshes "
              << material.settings.used_with_instanced_static_meshes << '\n';

    std::cout << "parameters " << material.parameters.size() << '\n';
    for (std::size_t index{}; index < material.parameters.size(); ++index) {
        auto const& parameter{material.parameters[index]};
        std::cout << "  [" << index << "] " << parameter.name << ' ' << type_name(parameter.type)
                  << " node=" << parameter.node.index;
        if (parameter.type == material_synth::ValueType::texture) {
            std::cout << " default=" << parameter.texture_path;
        } else {
            std::cout << " default=[";
            auto const components{material_synth::component_count(parameter.type)};
            for (std::size_t component{}; component < components; ++component) {
                std::cout << (component == 0 ? "" : ",") << parameter.default_value[component];
            }
            std::cout << ']';
        }
        std::cout << '\n';
    }

    std::cout << "nodes " << material.nodes.size() << '\n';
    for (std::size_t index{}; index < material.nodes.size(); ++index) {
        auto const& node{material.nodes[index]};
        std::cout << "  [" << index << "] " << kind_name(node.kind) << ' ' << type_name(node.type)
                  << " inputs=";
        for (std::size_t input{}; input < node.inputs.size(); ++input) {
            std::cout << (input == 0 ? "[" : ",") << node.inputs[input].index;
        }
        std::cout << (node.inputs.empty() ? "[]" : "]");
        if (node.kind == material_synth::NodeKind::constant) {
            std::cout << " value=[";
            for (std::size_t component{}; component < node.component_count; ++component) {
                std::cout << (component == 0 ? "" : ",") << node.constant[component];
            }
            std::cout << ']';
        } else if (node.kind == material_synth::NodeKind::parameter) {
            std::cout << " parameter=" << node.parameter_index;
        } else if (node.kind == material_synth::NodeKind::texture_coordinate) {
            std::cout << " coordinate=" << node.coordinate_index;
        } else if (node.kind == material_synth::NodeKind::per_instance_custom_data) {
            std::cout << " data-index=" << node.instance_data_index;
        } else if (node.kind == material_synth::NodeKind::custom) {
            std::cout << " description=" << std::quoted(node.description)
                      << " code=" << std::quoted(node.code);
        }
        std::cout << " @ " << node.span.path << ':' << node.span.line << ':' << node.span.column
                  << '\n';
        for (auto const& custom_input : node.custom_inputs) {
            std::cout << "    custom-input " << custom_input.name << ' '
                      << type_name(custom_input.type) << " node=" << custom_input.node.index
                      << '\n';
        }
    }

    for (auto const& binding : material.bindings) {
        std::cout << "binding " << binding.name << " node=" << binding.node.index << '\n';
    }
    for (auto const& output : material.outputs) {
        std::cout << "output " << output.name << " node=" << output.node.index << '\n';
    }
    for (auto const& dependency : material.texture_dependencies) {
        std::cout << "texture " << dependency << '\n';
    }
}

}

auto lispb::run_material_cli(int const argc, char const* const* const argv) -> int {
    if (argc == 2 && std::string_view{argv[1]} == "--help") {
        print_usage(std::cout);
        return 0;
    }

    auto const options{parse_options(argc, argv)};
    if (!options) {
        print_usage(std::cerr);
        return 2;
    }

    std::error_code error;
    auto const input{fs::absolute(options->input, error)};
    auto const source{error ? std::nullopt : read_file(input)};
    if (!source) {
        std::cerr << "lispb: unable to read input file '" << options->input.string() << "'\n";
        return 1;
    }

    auto const project_root{options->project_root ? find_project_root(*options->project_root)
                                                  : find_project_root(input.parent_path())};
    if (!project_root) {
        std::cerr << "lispb: unable to find a directory containing a .uproject; use "
                     "--project-root\n";
        return 1;
    }

    std::error_code relative_error;
    auto const relative_input{fs::relative(input, *project_root, relative_error)};
    auto const source_path{relative_error ? input.generic_string()
                                          : relative_input.generic_string()};
    ProjectTextureResolver resolver{.project_root = *project_root};
    auto const analysis{
        material_synth::analyze(source_path, *source, {&resolver, resolve_texture})};
    for (auto const& diagnostic : analysis.diagnostics) {
        std::cerr << diagnostic.path << ':' << diagnostic.line << ':' << diagnostic.column
                  << ": error: " << diagnostic.message << '\n';
    }
    if (!analysis.material) {
        std::cout << "LISPB_RESULT status=failure command=" << options->command
                  << " errors=" << analysis.diagnostics.size() << '\n';
        return 1;
    }

    if (options->command == "dump-ir") {
        dump_ir(*analysis.material);
    } else if (options->command == "compile") {
        auto const source_bytes{
            std::span{reinterpret_cast<std::uint8_t const*>(source->data()), source->size()}};
        auto const artifact{
            material_synth::serialize({.source_path = source_path,
                                       .source_hash = material_synth::sha256(source_bytes),
                                       .material = std::move(*analysis.material)})};
        if (!artifact) {
            std::cerr << "lispb: unable to serialize compiled material: " << artifact.error()
                      << '\n';
            std::cout << "LISPB_RESULT status=failure command=compile errors=1\n";
            return 1;
        }
        if (!write_file(*options->output, *artifact)) {
            std::cerr << "lispb: unable to write output file '" << options->output->string()
                      << "'\n";
            std::cout << "LISPB_RESULT status=failure command=compile errors=1\n";
            return 1;
        }
        if (options->depfile &&
            !write_depfile(*options->depfile, *options->output, input, resolver.dependencies)) {
            std::cerr << "lispb: unable to write depfile '" << options->depfile->string() << "'\n";
            std::cout << "LISPB_RESULT status=failure command=compile errors=1\n";
            return 1;
        }
    }
    std::cout << "LISPB_RESULT status=success command=" << options->command
              << " asset=" << analysis.material->settings.package_path << " errors=0\n";
    return 0;
}
