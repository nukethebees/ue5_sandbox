#pragma once

#include "syntax.h"

#include <codegen/generated_file.h>

#include <vector>

namespace kernel_codegen::detail {

auto render(KernelModule const& module) -> std::vector<codegen::GeneratedFile>;

}
