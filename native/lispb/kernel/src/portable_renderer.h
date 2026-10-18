#pragma once

#include "lowering.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/indices.h>
#include <kernel_codegen/indices.h>

namespace kernel_codegen::detail {

auto lower_portable(lispb::codegen::CppIR& cpp,
                    ModuleId const module,
                    EmissionId const emission,
                    Document const& document) -> lispb::codegen::ModuleId;

}
