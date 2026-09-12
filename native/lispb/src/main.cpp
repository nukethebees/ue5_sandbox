#include <codegen/generator.h>
#include <codegen/manifest.h>
#include <kernel_codegen/compiler.h>
#include <lispb/project.h>
#include <slate_codegen/compiler.h>

#include "../material/cli/material_cli.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace {

namespace fs = std::filesystem;

enum class Action { generate, check, validate, expand, dump_ir };

struct Arguments {
    Action action;
    std::string target;
    fs::path project{"lispb/project.lispb"};
    fs::path build_root{fs::current_path()};
    std::optional<fs::path> depfile;
};

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        auto const base{fs::temp_directory_path()};
        for (unsigned int attempt{}; attempt < 100; ++attempt) {
            path_ = base /
                    ("lispb-" +
                     std::to_string(fs::file_time_type::clock::now().time_since_epoch().count()) +
                     "-" + std::to_string(attempt));
            if (fs::create_directory(path_)) {
                return;
            }
        }
        throw std::runtime_error{"Unable to create a temporary directory"};
    }

    ~TemporaryDirectory() {
        std::error_code error;
        fs::remove_all(path_, error);
    }

    TemporaryDirectory(TemporaryDirectory const&) = delete;
    auto operator=(TemporaryDirectory const&) -> TemporaryDirectory& = delete;

    [[nodiscard]] auto path() const -> fs::path const& { return path_; }
  private:
    fs::path path_;
};

void print_usage(std::ostream& stream) {
    stream << "usage: lispb <generate|check|validate|expand|dump-ir> --target <name>\n"
              "             [--project <file>] [--build-root <directory>] [--depfile <file>]\n";
}

auto parse_action(std::string_view const value) -> Action {
    if (value == "generate") {
        return Action::generate;
    }
    if (value == "check") {
        return Action::check;
    }
    if (value == "validate") {
        return Action::validate;
    }
    if (value == "expand") {
        return Action::expand;
    }
    if (value == "dump-ir") {
        return Action::dump_ir;
    }
    throw std::invalid_argument{"Unknown action: " + std::string{value}};
}

auto parse_arguments(int const argc, char const* const* const argv) -> Arguments {
    if (argc < 2) {
        throw std::invalid_argument{"Missing action"};
    }

    Arguments result{.action = parse_action(argv[1])};
    std::set<std::string> seen;
    for (int index{2}; index < argc; ++index) {
        std::string const argument{argv[index]};
        if (!seen.insert(argument).second) {
            throw std::invalid_argument{"Duplicate argument: " + argument};
        }
        if (++index >= argc || std::string_view{argv[index]}.starts_with("--")) {
            throw std::invalid_argument{"Missing value after " + argument};
        }

        if (argument == "--target") {
            result.target = argv[index];
        } else if (argument == "--project") {
            result.project = argv[index];
        } else if (argument == "--build-root") {
            result.build_root = argv[index];
        } else if (argument == "--depfile") {
            result.depfile = fs::path{argv[index]};
        } else {
            throw std::invalid_argument{"Unknown argument: " + argument};
        }
    }
    if (result.target.empty()) {
        throw std::invalid_argument{"Missing --target"};
    }
    return result;
}

auto kernel_profile(lispb::KernelProfile const profile) -> kernel_codegen::Profile {
    switch (profile) {
        case lispb::KernelProfile::unreal:
            return kernel_codegen::Profile::unreal;
        case lispb::KernelProfile::standard:
            return kernel_codegen::Profile::standard;
        case lispb::KernelProfile::unreal_avx2_lab:
            return kernel_codegen::Profile::unreal_avx2_lab;
        case lispb::KernelProfile::native_x86_simd_lab:
            return kernel_codegen::Profile::native_x86_simd_lab;
    }
    throw std::invalid_argument{"Unknown kernel profile"};
}

auto material_command(Action const action) -> std::string_view {
    switch (action) {
        case Action::generate:
        case Action::check:
            return "compile";
        case Action::validate:
            return "validate";
        case Action::dump_ir:
            return "dump-ir";
        case Action::expand:
            break;
    }
    throw std::invalid_argument{"expand is only supported for Slate targets"};
}

auto run_material(std::string_view const command,
                  fs::path const& source,
                  std::optional<fs::path> const& artifact,
                  fs::path const& project_root,
                  std::optional<fs::path> const& depfile) -> int {
    std::vector<std::string> storage{"lispb", std::string{command}, "--input", source.string()};
    if (artifact) {
        storage.emplace_back("--output");
        storage.push_back(artifact->string());
    }
    storage.emplace_back("--project-root");
    storage.push_back(project_root.string());
    if (depfile) {
        storage.emplace_back("--depfile");
        storage.push_back(depfile->string());
    }
    std::vector<char const*> arguments;
    arguments.reserve(storage.size());
    for (auto const& value : storage) {
        arguments.push_back(value.c_str());
    }
    return lispb::run_material_cli(static_cast<int>(arguments.size()), arguments.data());
}

auto equal_files(fs::path const& first, fs::path const& second) -> bool {
    std::ifstream left{first, std::ios::binary};
    std::ifstream right{second, std::ios::binary};
    return left && right &&
           std::equal(std::istreambuf_iterator<char>{left},
                      std::istreambuf_iterator<char>{},
                      std::istreambuf_iterator<char>{right},
                      std::istreambuf_iterator<char>{});
}

auto run_target(lispb::Target const& target,
                Action const action,
                fs::path const& project_root,
                fs::path const& build_root,
                std::optional<fs::path> const& depfile) -> int {
    return std::visit(
        [&](auto const& value) -> int {
            using Target = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Target, lispb::CppSchemaTarget>) {
                if (depfile) {
                    throw std::invalid_argument{
                        "--depfile is only supported for Material generation"};
                }
                if (action == Action::expand || action == Action::dump_ir) {
                    throw std::invalid_argument{"Action is not supported for C++ schema targets"};
                }
                std::vector<fs::path> sources;
                sources.reserve(value.sources.size());
                for (auto const& source : value.sources) {
                    sources.push_back(project_root / source);
                }
                auto const manifest{codegen::load_sources(project_root / value.types, sources)};
                auto const files{codegen::render_modules(codegen::lower_modules(manifest))};
                if (action == Action::validate) {
                    return 0;
                }
                return codegen::generate_files(
                    files,
                    project_root,
                    lispb::resolve(value.output_root, project_root, build_root),
                    action == Action::check);
            } else if constexpr (std::is_same_v<Target, lispb::SlateTarget>) {
                if (depfile) {
                    throw std::invalid_argument{
                        "--depfile is only supported for Material generation"};
                }
                if (action == Action::dump_ir) {
                    throw std::invalid_argument{"dump-ir is only supported for Material targets"};
                }
                auto options{slate_codegen::SourceOptions{
                    .source_root = project_root,
                    .inputs = value.sources,
                    .include_directories = value.include_directories,
                    .output_root = lispb::resolve(value.output_root, project_root, build_root),
                    .check = action == Action::check}};
                if (action == Action::expand) {
                    std::cout << slate_codegen::expand_sources(options);
                    return 0;
                }
                if (action == Action::validate) {
                    TemporaryDirectory temporary;
                    options.output_root = temporary.path();
                    return slate_codegen::compile_sources(options);
                }
                return slate_codegen::compile_sources(options);
            } else if constexpr (std::is_same_v<Target, lispb::KernelTarget>) {
                if (depfile) {
                    throw std::invalid_argument{
                        "--depfile is only supported for Material generation"};
                }
                if (action == Action::expand || action == Action::dump_ir) {
                    throw std::invalid_argument{"Action is not supported for Kernel targets"};
                }
                auto options{kernel_codegen::SourceOptions{
                    .source_root = project_root,
                    .inputs = value.sources,
                    .output_root = lispb::resolve(value.output_root, project_root, build_root),
                    .profile = kernel_profile(value.profile),
                    .check = action == Action::check}};
                if (action == Action::validate) {
                    TemporaryDirectory temporary;
                    options.output_root = temporary.path();
                    return kernel_codegen::compile_sources(options);
                }
                return kernel_codegen::compile_sources(options);
            } else {
                if (depfile && action != Action::generate) {
                    throw std::invalid_argument{
                        "--depfile is only supported for Material generation"};
                }
                auto const source{project_root / value.source};
                auto const artifact{lispb::resolve(value.artifact, project_root, build_root)};
                if (action != Action::check) {
                    return run_material(material_command(action),
                                        source,
                                        action == Action::generate
                                            ? std::optional<fs::path>{artifact}
                                            : std::nullopt,
                                        project_root,
                                        depfile);
                }
                TemporaryDirectory temporary;
                auto const generated{temporary.path() / artifact.filename()};
                auto const result{
                    run_material("compile", source, generated, project_root, std::nullopt)};
                if (result != 0) {
                    return result;
                }
                if (!equal_files(generated, artifact)) {
                    std::cerr << artifact.string() << " is stale or missing\n";
                    return 1;
                }
                return 0;
            }
        },
        target);
}

} // namespace

auto main(int const argc, char const* const* const argv) -> int {
    try {
        if (argc == 2 && std::string_view{argv[1]} == "--help") {
            print_usage(std::cout);
            return 0;
        }
        auto const arguments{parse_arguments(argc, argv)};
        auto const project{lispb::load_project(arguments.project)};
        auto const targets{lispb::expand_target(project, arguments.target)};
        if ((arguments.action == Action::expand || arguments.action == Action::dump_ir) &&
            targets.size() != 1) {
            throw std::invalid_argument{"expand and dump-ir require a single target"};
        }
        if (arguments.depfile && targets.size() != 1) {
            throw std::invalid_argument{"--depfile requires a single target"};
        }

        for (auto const& name : targets) {
            auto const result{run_target(project.targets.at(name),
                                         arguments.action,
                                         project.root,
                                         fs::absolute(arguments.build_root).lexically_normal(),
                                         arguments.depfile)};
            if (result != 0) {
                return result;
            }
        }
        return 0;
    } catch (std::exception const& error) {
        std::cerr << "lispb: " << error.what() << '\n';
        print_usage(std::cerr);
        return 2;
    }
}
