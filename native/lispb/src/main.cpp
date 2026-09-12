#include <codegen/generator.h>
#include <kernel_codegen/compiler.h>
#include <lispb/material_compiler.h>
#include <lispb/output.h>
#include <lispb/project.h>
#include <slate_codegen/compiler.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
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

struct CompiledTarget {
    lispb::Compilation compilation;
    lispb::PublicationOptions publication;
};

auto compile_target(lispb::Target const& target,
                    fs::path const& project_root,
                    fs::path const& build_root) -> CompiledTarget {
    return std::visit(
        [&](auto const& value) -> CompiledTarget {
            using Target = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Target, lispb::CppSchemaTarget>) {
                std::vector<fs::path> sources;
                sources.reserve(value.sources.size());
                for (auto const& source : value.sources) {
                    sources.push_back(project_root / source);
                }
                auto compilation{codegen::compile_sources(project_root / value.types, sources)};
                return {.compilation = std::move(compilation),
                        .publication = {.path_base = project_root,
                                        .output_root = lispb::resolve(
                                            value.output_root, project_root, build_root)}};
            } else if constexpr (std::is_same_v<Target, lispb::SlateTarget>) {
                auto const output_root{lispb::resolve(value.output_root, project_root, build_root)};
                auto compilation{slate_codegen::compile_sources({
                    .source_root = project_root,
                    .inputs = value.sources,
                    .include_directories = value.include_directories,
                })};
                return {.compilation = std::move(compilation),
                        .publication = {.path_base = output_root, .output_root = output_root}};
            } else if constexpr (std::is_same_v<Target, lispb::KernelTarget>) {
                auto const output_root{lispb::resolve(value.output_root, project_root, build_root)};
                auto compilation{kernel_codegen::compile_sources({
                    .source_root = project_root,
                    .inputs = value.sources,
                    .profile = kernel_profile(value.profile),
                })};
                return {.compilation = std::move(compilation),
                        .publication = {.path_base = output_root, .output_root = output_root}};
            } else {
                auto const source{project_root / value.source};
                auto const artifact{lispb::resolve(value.artifact, project_root, build_root)};
                return {.compilation =
                            lispb::compile_material(source, project_root, artifact.filename()),
                        .publication = {.path_base = artifact.parent_path(),
                                        .output_root = artifact.parent_path(),
                                        .track_outputs = false}};
            }
        },
        target);
}

auto expand_target(lispb::Target const& target, fs::path const& project_root) -> std::string {
    auto const* slate{std::get_if<lispb::SlateTarget>(&target)};
    if (slate == nullptr) {
        throw std::invalid_argument{"expand is only supported for Slate targets"};
    }
    return slate_codegen::expand_sources({.source_root = project_root,
                                          .inputs = slate->sources,
                                          .include_directories = slate->include_directories});
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
        if (arguments.depfile && arguments.action != Action::generate) {
            throw std::invalid_argument{"--depfile is only supported with generate"};
        }

        if (arguments.action == Action::expand) {
            std::cout << expand_target(project.targets.at(targets.front()), project.root);
            return 0;
        }

        for (auto const& name : targets) {
            auto compiled{compile_target(project.targets.at(name),
                                         project.root,
                                         fs::absolute(arguments.build_root).lexically_normal())};
            compiled.compilation.dependencies.push_back(
                fs::absolute(arguments.project).lexically_normal());

            if (arguments.action == Action::validate) {
                continue;
            }
            if (arguments.action == Action::dump_ir) {
                if (!compiled.compilation.debug_dump) {
                    throw std::invalid_argument{"dump-ir is only supported for Material targets"};
                }
                std::cout << *compiled.compilation.debug_dump;
                continue;
            }

            compiled.publication.check_only = arguments.action == Action::check;
            auto const result{lispb::publish(compiled.compilation, compiled.publication)};
            if (result != 0) {
                return result;
            }
            if (arguments.depfile) {
                lispb::write_depfile(
                    *arguments.depfile, compiled.compilation, compiled.publication);
            }
        }
        return 0;
    } catch (std::exception const& error) {
        std::cerr << "lispb: " << error.what() << '\n';
        print_usage(std::cerr);
        return 2;
    }
}
