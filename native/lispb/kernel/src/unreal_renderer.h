#pragma once

#include "lowering.h"

#include <string>
#include <vector>

namespace kernel_codegen::detail {

auto render_unreal_header(Emission const& emission, std::vector<ExpandedVariant> const& variants)
    -> std::string;
auto render_unreal_source(KernelModule const& module,
                          Emission const& emission,
                          std::vector<ExpandedVariant> const& variants) -> std::string;

}
