#include "renderer.h"

#include "lowering.h"
#include "standard_renderer.h"
#include "test_renderer.h"
#include "unreal_renderer.h"

#include <algorithm>

namespace kernel_codegen::detail {

auto render(KernelModule const& module, Profile const profile)
    -> std::vector<codegen::GeneratedFile> {
    auto const emission{std::ranges::find_if(module.emissions, [&](auto const& candidate) {
        return candidate.profile == profile;
    })};
    if (emission == module.emissions.end()) {
        return {};
    }

    auto const variants{expand(module)};
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
