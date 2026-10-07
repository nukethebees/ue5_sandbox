#pragma once

#include <ioj/s7/ast.h>

struct s7_scheme;
struct s7_cell;

namespace ioj::s7::detail {
// The caller must GC-protect the value for the duration of this call.
[[nodiscard]] auto materialize_ast(s7_scheme& scheme, s7_cell* value, AstLimits limits)
    -> AstResult;
}
