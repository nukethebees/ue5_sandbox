#include <kernel_codegen/compiler.h>

#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

auto parse_arguments(int argc, char const* const* argv) -> kernel_codegen::CompileOptions {
    kernel_codegen::CompileOptions result;
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
        } else if (argument == "--profile") {
            std::filesystem::path value;
            read_path(value);
            if (value == "unreal") {
                result.profile = kernel_codegen::Profile::unreal;
            } else if (value == "standard") {
                result.profile = kernel_codegen::Profile::standard;
            } else if (value == "unreal-avx2-lab") {
                result.profile = kernel_codegen::Profile::unreal_avx2_lab;
            } else if (value == "native-x86-simd-lab") {
                result.profile = kernel_codegen::Profile::native_x86_simd_lab;
            } else {
                throw std::invalid_argument{"Unknown kernel profile: " + value.string()};
            }
        } else {
            throw std::invalid_argument{"Unknown argument: " + argument};
        }
    }
    if (result.manifest.empty()) {
        throw std::invalid_argument{"--manifest is required"};
    }
    if (!seen.contains("--profile")) {
        throw std::invalid_argument{"--profile is required"};
    }
    return result;
}

}

auto main(int argc, char const* const* argv) -> int {
    try {
        auto const result{kernel_codegen::compile_manifest(parse_arguments(argc, argv))};
        if (result == 1) {
            std::cout << "Run the generate-kernel-code CMake target.\n";
        }
        return result;
    } catch (std::exception const& error) {
        std::cerr << "kernelc: " << error.what() << '\n';
        return 2;
    }
}
