#pragma once

#include "s7.h"

namespace ioj::s7::detail {
inline constexpr char source_variable_name[]{"*sandbox-s7-source*"};

// Protect each returned helper before allocating another s7 object.
[[nodiscard]] auto prepare_safe_format(s7_scheme* scheme) -> s7_pointer;
[[nodiscard]] auto prepare_disabled_operation(s7_scheme* scheme) -> s7_pointer;
void restrict_global_environment(s7_scheme& scheme, s7_pointer disabled);
void install_binding(s7_scheme* scheme, char const* name, s7_pointer value);
}
