#include <codegen/generator.h>
#include <codegen/json.h>

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

struct Arguments {
    std::filesystem::path manifest{"Codegen/manifests/manifest.json"};
    std::filesystem::path project_root{std::filesystem::current_path()};
    std::optional<std::filesystem::path> output_root;
    bool check{false};
};

auto parse_arguments(int argc, char const* const* argv) -> Arguments {
    Arguments result;
    for (int index{1}; index < argc; ++index) {
        std::string const argument{argv[index]};
        if (argument == "--check") {
            result.check = true;
            continue;
        }
        auto read_path = [&](std::filesystem::path& destination) {
            if (++index >= argc) {
                throw std::invalid_argument{"Missing value after " + argument};
            }
            destination = argv[index];
        };
        if (argument == "--manifest") {
            read_path(result.manifest);
        } else if (argument == "--project-root") {
            read_path(result.project_root);
        } else if (argument == "--output-root") {
            std::filesystem::path output;
            read_path(output);
            result.output_root = std::move(output);
        } else {
            throw std::invalid_argument{"Unknown argument: " + argument};
        }
    }
    return result;
}

} // namespace

auto main(int argc, char const* const* argv) -> int {
    try {
        auto const arguments{parse_arguments(argc, argv)};
        auto const manifest_path{arguments.manifest.is_absolute()
                                     ? arguments.manifest
                                     : arguments.project_root / arguments.manifest};
        auto const output_root{arguments.output_root.value_or(arguments.project_root)};
        auto const manifest{codegen::load_manifest(manifest_path)};
        auto const files{codegen::render_modules(codegen::lower_modules(manifest))};
        return codegen::generate_files(
            files, arguments.project_root, output_root, arguments.check);
    } catch (std::exception const& error) {
        std::cerr << "codegen: " << error.what() << '\n';
        return 2;
    }
}
