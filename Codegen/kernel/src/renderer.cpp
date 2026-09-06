#include "renderer.h"

#include "avx2_lab_renderer.h"
#include "lowering.h"
#include "standard_renderer.h"
#include "test_renderer.h"
#include "unreal_renderer.h"

#include <algorithm>
#include <stdexcept>

namespace kernel_codegen::detail {
namespace {

auto selected_variant(std::vector<ExpandedVariant> const& variants,
                      Emission const& emission) -> ExpandedVariant const& {
    auto const& selection{*emission.selection};
    auto const found{std::ranges::find_if(variants, [&](auto const& candidate) {
        return candidate.operation->name == selection.operation &&
               candidate.type == selection.type && candidate.storage == selection.storage &&
               candidate.variant->kind == selection.variant;
    })};
    if (found == variants.end()) {
        throw std::invalid_argument{"SIMD lab selection did not match a generated variant"};
    }
    return *found;
}

}

auto render(KernelModule const& module, Profile const profile)
    -> std::vector<codegen::GeneratedFile> {
    auto const emission{std::ranges::find_if(module.emissions, [&](auto const& candidate) {
        return candidate.profile == profile;
    })};
    if (emission == module.emissions.end()) {
        return {};
    }

    auto const variants{expand(module)};
    if (profile == Profile::unreal_avx2_lab) {
        auto const& selected{selected_variant(variants, *emission)};
        return {codegen::GeneratedFile{emission->header,
                                       render_avx2_lab_header(*emission, selected)},
                codegen::GeneratedFile{emission->source,
                                       render_avx2_lab_source(*emission, selected)}};
    }
    if (profile == Profile::native_x86_simd_lab) {
        auto const& selected{selected_variant(variants, *emission)};
        std::vector<codegen::GeneratedFile> result{
            codegen::GeneratedFile{emission->header,
                                   render_native_simd_lab_header(*emission, selected)},
            codegen::GeneratedFile{emission->source,
                                   render_native_avx2_lab_source(*emission, selected)},
            codegen::GeneratedFile{*emission->avx512_source,
                                   render_native_avx512_lab_source(*emission, selected)}};
        if (emission->dispatch_source) {
            result.push_back(codegen::GeneratedFile{
                *emission->dispatch_source,
                render_native_simd_dispatch_source(*emission, selected)});
        }
        if (emission->relaxed_avx2_source) {
            result.push_back(codegen::GeneratedFile{
                *emission->relaxed_avx2_source,
                render_native_relaxed_autovec_source(*emission, selected, "avx2")});
            result.push_back(codegen::GeneratedFile{
                *emission->relaxed_avx512_source,
                render_native_relaxed_autovec_source(*emission, selected, "avx512")});
        }
        return result;
    }
    if (profile == Profile::standard) {
        std::vector<codegen::GeneratedFile> result{
            codegen::GeneratedFile{emission->header, render_standard_header(*emission, variants)},
            codegen::GeneratedFile{
                emission->source, render_standard_source(module, *emission, variants)}};
        if (emission->tests) {
            result.push_back(codegen::GeneratedFile{
                *emission->tests, render_standard_tests(*emission, module, variants)});
        }
        return result;
    }

    return {codegen::GeneratedFile{emission->header, render_unreal_header(*emission, variants)},
            codegen::GeneratedFile{emission->source,
                                   render_unreal_source(module, *emission, variants)}};
}

}
