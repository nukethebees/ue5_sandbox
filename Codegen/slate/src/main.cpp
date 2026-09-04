#include <slate_codegen/compiler.h>

#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

auto parse_arguments(int argc, char const* const* argv) -> slate_codegen::CompileOptions {
    slate_codegen::CompileOptions result;
    std::set<std::string> seen;
    for (int index{1}; index < argc; ++index) {
        std::string const argument{argv[index]};
        if (!seen.insert(argument).second) {
            throw std::invalid_argument{"Duplicate argument: " + argument};
        }
        if (argument == "--check") {
            result.check = true;
            continue;
        }
        auto read_path = [&](std::filesystem::path& destination) {
            if (++index >= argc || std::string_view{argv[index]}.starts_with("--")) {
                throw std::invalid_argument{"Missing value after " + argument};
            }
            destination = argv[index];
        };
        if (argument == "--manifest") {
            read_path(result.manifest);
        } else if (argument == "--output-root") {
            std::filesystem::path output_root;
            read_path(output_root);
            result.output_root = std::move(output_root);
        } else {
            throw std::invalid_argument{"Unknown argument: " + argument};
        }
    }
    if (result.manifest.empty()) {
        throw std::invalid_argument{"--manifest is required"};
    }
    return result;
}

}

auto main(int argc, char const* const* argv) -> int {
    try {
        auto const options{parse_arguments(argc, argv)};
        auto const result{slate_codegen::compile_manifest(options)};
        if (result == 1) {
            std::cout << "Run the generate-slate-code CMake target.\n";
        }
        return result;
    } catch (std::exception const& error) {
        std::cerr << "slatec: " << error.what() << '\n';
        return 2;
    }
}
