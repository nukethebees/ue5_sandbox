#pragma once

#include "validation.h"

#include <kernel_codegen/document.h>
#include <kernel_codegen/indices.h>
#include <lispb/compilation.h>
#include <lispb/text_artifact.h>

#include <vector>

namespace lispb::codegen {
class CppIR;
}

namespace kernel_codegen::detail {

// Consume analysis for this document; no source validation occurs during lowering.
auto render(lispb::codegen::CppIR& cpp, Document const& document, ModuleAnalysis const& analysis)
    -> std::vector<lispb::TextArtifact>;

}
