#include <kernel_codegen/compiler.h>

#include "parser.h"
#include "renderer.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/reader.h>
#include <codegen/sexpr/syntax.h>
#include <kernel_codegen/document.h>
#include <lispb/compilation.h>
#include <lispb/kernel_profile.h>

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace kernel_codegen {

void compile(lispb::codegen::sexpr::Syntax& syntax,
             detail::Document& document,
             lispb::codegen::CppIR& cpp,
             lispb::Compilation& output,
             std::span<lispb::codegen::sexpr::SourceFileId const> const inputs,
             lispb::KernelProfile const profile,
             bool const generate) {
    auto const first_module{document.modules.size()};
    for (auto const input : inputs) {
        auto const forms{lispb::codegen::sexpr::read_forms(syntax, input)};
        detail::parse(syntax, document, input, forms);
    }

    auto const module_count{document.modules.size()};
    std::vector<detail::ModuleAnalysis> analyses;
    analyses.reserve(module_count - first_module);
    for (auto index{first_module}; index < module_count; ++index) {
        analyses.push_back(detail::analyze(
            syntax, document, detail::ModuleId{static_cast<std::uint32_t>(index)}, profile));
    }
    if (!generate) {
        return;
    }

    for (auto const& analysis : analyses) {
        auto rendered{detail::render(cpp, document, analysis)};
        for (auto& artifact : rendered) {
            output.add_text(
                std::move(artifact.path), std::move(artifact.content), artifact.format_generated);
        }
    }
}

}
