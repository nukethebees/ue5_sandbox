#include <lispb/output.h>
#include <lispb/project.h>
#include <lispb/target_compiler.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
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
            std::cout << lispb::expand_target_source(project.targets.at(targets.front()),
                                                     project.root);
            return 0;
        }

        for (auto const& name : targets) {
            auto compiled{
                lispb::compile_target(project.targets.at(name),
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
