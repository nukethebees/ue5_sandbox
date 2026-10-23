#include "renderer.h"

#include "portable_renderer.h"
#include "test_renderer.h"

#include <codegen/ast/cpp_files.h>
#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_renderer.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/modules.h>
#include <kernel_codegen/document.h>
#include <lispb/strings.h>
#include <lispb/text_artifact.h>

#include <filesystem>
#include <vector>

namespace kernel_codegen::detail {
auto render(lispb::codegen::CppIR& cpp, Document const& document, ModuleAnalysis const& analysis)
    -> std::vector<lispb::TextArtifact> {
    if (!analysis.emission) {
        return {};
    }

    auto const emission{*analysis.emission};
    auto const generated{lower_portable(cpp, analysis.module, emission, document)};
    auto const generated_index{cpp.modules.index(generated)};
    std::vector<lispb::codegen::CppFileId> files{cpp.modules.header[generated_index],
                                                 cpp.modules.source[generated_index]};
    if (document.emissions.tests(emission)) {
        files.push_back(
            lower_standard_tests(cpp, emission, analysis.module, document, analysis.fixtures));
    }

    std::vector<lispb::TextArtifact> result;
    result.reserve(files.size());
    for (auto const file : files) {
        auto const index{cpp.files.index(file)};
        result.push_back(
            lispb::TextArtifact{std::filesystem::path{cpp.strings.view(cpp.files.path[index])},
                                lispb::codegen::CppRenderer{cpp}.render(file),
                                cpp.files.flags[index].format_generated()});
    }
    return result;
}
}
