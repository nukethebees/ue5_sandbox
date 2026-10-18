#pragma once

#include "reference_evaluator.h"

#include <kernel_codegen/document.h>
#include <kernel_codegen/indices.h>
#include <lispb/kernel_profile.h>

#include <optional>
#include <vector>

namespace lispb::codegen::sexpr {
class Syntax;
}

namespace kernel_codegen::detail {
struct ModuleAnalysis {
    ModuleId module;
    std::optional<EmissionId> emission;
    std::vector<ReferenceFixture> fixtures;
};

// Validate the selected emission and prepare any requested reference fixtures before lowering.
auto analyze(lispb::codegen::sexpr::Syntax& syntax,
             Document const& document,
             ModuleId module,
             lispb::KernelProfile profile) -> ModuleAnalysis;
}
