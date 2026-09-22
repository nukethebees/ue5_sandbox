#include <lispb/target_compiler.h>

#include <codegen/generator.h>
#include <kernel_codegen/compiler.h>
#include <lispb/material_compiler.h>
#include <slate_codegen/compiler.h>

#include <stdexcept>
#include <type_traits>
#include <variant>
#include <vector>

namespace lispb {
namespace {

auto kernel_profile(KernelProfile const profile) -> kernel_codegen::Profile {
    switch (profile) {
        case KernelProfile::unreal:
            return kernel_codegen::Profile::unreal;
        case KernelProfile::standard:
            return kernel_codegen::Profile::standard;
        case KernelProfile::unreal_avx2_lab:
            return kernel_codegen::Profile::unreal_avx2_lab;
        case KernelProfile::native_x86_simd_lab:
            return kernel_codegen::Profile::native_x86_simd_lab;
    }
    throw std::invalid_argument{"Unknown kernel profile"};
}

} // namespace

auto compile_target(Target const& target,
                    std::filesystem::path const& project_root,
                    std::filesystem::path const& build_root) -> CompiledTarget {
    return std::visit(
        [&](auto const& value) -> CompiledTarget {
            using TargetType = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<TargetType, CppSchemaTarget>) {
                std::vector<std::filesystem::path> sources;
                sources.reserve(value.sources.size());
                for (auto const& source : value.sources) {
                    sources.push_back(project_root / source);
                }
                auto compilation{codegen::compile_sources(project_root / value.types, sources)};
                return {.compilation = std::move(compilation),
                        .publication = {.path_base = project_root,
                                        .output_root =
                                            resolve(value.output_root, project_root, build_root)}};
            } else if constexpr (std::is_same_v<TargetType, SlateTarget>) {
                auto const output_root{resolve(value.output_root, project_root, build_root)};
                auto compilation{slate_codegen::compile_sources({
                    .source_root = project_root,
                    .inputs = value.sources,
                    .include_directories = value.include_directories,
                })};
                return {.compilation = std::move(compilation),
                        .publication = {.path_base = output_root, .output_root = output_root}};
            } else if constexpr (std::is_same_v<TargetType, KernelTarget>) {
                auto const output_root{resolve(value.output_root, project_root, build_root)};
                auto compilation{kernel_codegen::compile_sources({
                    .source_root = project_root,
                    .inputs = value.sources,
                    .profile = kernel_profile(value.profile),
                })};
                return {.compilation = std::move(compilation),
                        .publication = {.path_base = output_root, .output_root = output_root}};
            } else {
                auto const source{project_root / value.source};
                auto const artifact{resolve(value.artifact, project_root, build_root)};
                return {.compilation = compile_material(source, project_root, artifact.filename()),
                        .publication = {.path_base = artifact.parent_path(),
                                        .output_root = artifact.parent_path(),
                                        .track_outputs = false}};
            }
        },
        target);
}

auto expand_target_source(Target const& target, std::filesystem::path const& project_root)
    -> std::string {
    auto const* slate{std::get_if<SlateTarget>(&target)};
    if (slate == nullptr) {
        throw std::invalid_argument{"expand is only supported for Slate targets"};
    }
    return slate_codegen::expand_sources({.source_root = project_root,
                                          .inputs = slate->sources,
                                          .include_directories = slate->include_directories});
}

} // namespace lispb
