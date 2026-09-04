#include <slate_codegen/compiler.h>

#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

struct CommandOptions {
    slate_codegen::CompileOptions compile;
    bool expand{false};
};

auto parse_arguments(int argc, char const* const* argv) -> CommandOptions {
    CommandOptions result;
    std::set<std::string> seen;
    for (int index{1}; index < argc; ++index) {
        std::string const argument{argv[index]};
        if (!seen.insert(argument).second) {
            throw std::invalid_argument{"Duplicate argument: " + argument};
        }
        if (argument == "--check") {
            result.compile.check = true;
            continue;
        }
        if (argument == "--expand") {
            result.expand = true;
            continue;
        }
        auto read_path = [&](std::filesystem::path& destination) {
            if (++index >= argc || std::string_view{argv[index]}.starts_with("--")) {
                throw std::invalid_argument{"Missing value after " + argument};
            }
            destination = argv[index];
        };
        if (argument == "--manifest") {
            read_path(result.compile.manifest);
        } else if (argument == "--output-root") {
            std::filesystem::path output_root;
            read_path(output_root);
            result.compile.output_root = std::move(output_root);
        } else {
            throw std::invalid_argument{"Unknown argument: " + argument};
        }
    }
    if (result.compile.manifest.empty()) {
        throw std::invalid_argument{"--manifest is required"};
    }
    if (result.expand && (result.compile.check || result.compile.output_root)) {
        throw std::invalid_argument{"--expand cannot be combined with --check or --output-root"};
    }
    return result;
}

}

auto main(int argc, char const* const* argv) -> int {
    try {
        auto const options{parse_arguments(argc, argv)};
        if (options.expand) {
            std::cout << slate_codegen::expand_manifest(options.compile.manifest);
            return 0;
        }
        auto const result{slate_codegen::compile_manifest(options.compile)};
        if (result == 1) {
            std::cout << "Run the generate-slate-code CMake target.\n";
        }
        return result;
    } catch (std::exception const& error) {
        std::cerr << "slatec: " << error.what() << '\n';
        return 2;
    }
}
