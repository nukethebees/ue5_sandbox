#include <kernel_codegen/compiler.h>

#include "parser.h"
#include "renderer.h"

#include <codegen/sexpr/reader.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <utility>
#include <vector>

namespace kernel_codegen {
namespace {

auto lower_profile(Profile const profile) -> detail::Profile {
    switch (profile) {
        case Profile::unreal:
            return detail::Profile::unreal;
        case Profile::standard:
            return detail::Profile::standard;
        case Profile::unreal_avx2_lab:
            return detail::Profile::unreal_avx2_lab;
        case Profile::native_x86_simd_lab:
            return detail::Profile::native_x86_simd_lab;
    }
    throw std::invalid_argument{"Unknown kernel profile"};
}

auto read_file(std::filesystem::path const& path) -> std::string {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw std::runtime_error{"Cannot open Kernel source: " + path.string()};
    }
    return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

}

auto compile_sources(SourceOptions const& options) -> lispb::Compilation {
    auto const source_root{std::filesystem::absolute(options.source_root).lexically_normal()};
    auto const profile{lower_profile(options.profile)};
    lispb::Compilation result;
    for (auto const& input : options.inputs) {
        auto const input_path{source_root / input};
        result.dependencies.push_back(input_path.lexically_normal());
        auto const source{read_file(input_path)};
        auto const document{detail::parse(
            input.generic_string(), codegen::sexpr::read_forms(input.generic_string(), source))};
        for (auto const& module : document.modules) {
            auto rendered{detail::render(module, profile)};
            result.artifacts.insert(result.artifacts.end(),
                                    std::make_move_iterator(rendered.begin()),
                                    std::make_move_iterator(rendered.end()));
        }
    }
    return result;
}

}
