#pragma once

#include "lowering.h"
#include "reference_evaluator.h"

#include <codegen/ast/indices.h>
#include <kernel_codegen/indices.h>

#include <span>

namespace lispb::codegen {
class CppIR;
}

namespace kernel_codegen::detail {

auto lower_standard_tests(lispb::codegen::CppIR& cpp,
                          EmissionId const emission,
                          ModuleId const module,
                          Document const& document,
                          std::span<ReferenceFixture const> fixtures) -> lispb::codegen::CppFileId;

}
