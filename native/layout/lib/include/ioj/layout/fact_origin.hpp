#pragma once

#include <string_view>

namespace ioj::layout {

enum class FactOrigin { unspecified, target_abi, compiler_probe, manual_assumption, derived };

auto fact_origin_name(FactOrigin origin) -> std::string_view;

} // namespace ioj::layout
