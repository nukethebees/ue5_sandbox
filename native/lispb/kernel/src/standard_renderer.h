#pragma once

#include "lowering.h"

#include <string>
#include <vector>

namespace kernel_codegen::detail {

auto render_standard_header(Emission const& emission, std::vector<ExpandedVariant> const& variants)
    -> std::string;
auto render_standard_source(KernelModule const& module,
                            Emission const& emission,
                            std::vector<ExpandedVariant> const& variants) -> std::string;

}
