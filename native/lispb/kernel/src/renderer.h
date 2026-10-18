#pragma once

#include <kernel_codegen/document.h>
#include <kernel_codegen/indices.h>
#include <lispb/compilation.h>
#include <lispb/kernel_profile.h>
#include <lispb/text_artifact.h>

#include <vector>

namespace lispb::codegen {
class CppIR;
}

namespace kernel_codegen::detail {

void validate(Document const& document, ModuleId const module, lispb::KernelProfile profile);

auto render(lispb::codegen::CppIR& cpp,
            Document const& document,
            ModuleId const module,
            lispb::KernelProfile profile) -> std::vector<lispb::TextArtifact>;

}
