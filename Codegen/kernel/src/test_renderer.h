#pragma once

#include "lowering.h"

#include <string>
#include <vector>

namespace kernel_codegen::detail {

auto render_standard_tests(Emission const& emission,
                           KernelModule const& module,
                           std::vector<ExpandedVariant> const& variants) -> std::string;

}
