#include <material_gen/MaterialFrontend.h>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace {

namespace fs = std::filesystem;

struct Options {
    std::string_view command;
    fs::path input;
    std::optional<fs::path> project_root;
};

void print_usage(std::ostream& stream) {
    stream << "usage: materialc <validate|dump-ir> --input <file> "
              "[--project-root <directory>]\n";
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
        } else if (argument == "--project-root" && index + 1 < argc) {
            options.project_root = fs::path{argv[++index]};
        } else {
            return std::nullopt;
        }
    }
    if ((options.command != "validate" && options.command != "dump-ir") || options.input.empty()) {
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
    std::cout << "material " << material.settings.name << '\n'
              << "asset " << material.settings.package_path << '\n'
              << "domain ui\nblend additive\n";

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

int main(int const argc, char const* const* const argv) {
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
        std::cerr << "materialc: unable to read input file '" << options->input.string() << "'\n";
        return 1;
    }

    auto const project_root{options->project_root ? find_project_root(*options->project_root)
                                                  : find_project_root(input.parent_path())};
    if (!project_root) {
        std::cerr << "materialc: unable to find a directory containing a .uproject; use "
                     "--project-root\n";
        return 1;
    }

    ProjectTextureResolver resolver{.project_root = *project_root};
    auto const analysis{
        material_synth::analyze(input.generic_string(), *source, {&resolver, resolve_texture})};
    for (auto const& diagnostic : analysis.diagnostics) {
        std::cerr << diagnostic.path << ':' << diagnostic.line << ':' << diagnostic.column
                  << ": error: " << diagnostic.message << '\n';
    }
    if (!analysis.material) {
        std::cout << "MATERIALC_RESULT status=failure command=" << options->command
                  << " errors=" << analysis.diagnostics.size() << '\n';
        return 1;
    }

    if (options->command == "dump-ir") {
        dump_ir(*analysis.material);
    }
    std::cout << "MATERIALC_RESULT status=success command=" << options->command
              << " asset=" << analysis.material->settings.package_path << " errors=0\n";
    return 0;
}
