#pragma once

#include "lowering.h"

#include <string>

namespace kernel_codegen::detail {

auto render_avx2_lab_header(Emission const& emission, ExpandedVariant const& expanded)
    -> std::string;
auto render_avx2_lab_source(Emission const& emission, ExpandedVariant const& expanded)
    -> std::string;
auto render_native_simd_lab_header(Emission const& emission, ExpandedVariant const& expanded)
    -> std::string;
auto render_native_avx2_lab_source(Emission const& emission, ExpandedVariant const& expanded)
    -> std::string;
auto render_native_avx512_lab_source(Emission const& emission, ExpandedVariant const& expanded)
    -> std::string;
auto render_native_simd_dispatch_source(Emission const& emission, ExpandedVariant const& expanded)
    -> std::string;

}
