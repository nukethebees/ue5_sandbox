#pragma once
#include <codegen/sexpr/expr.h>

namespace lispb::codegen::sexpr {
struct CommonAtoms {
    Expr true_value;
    Expr false_value;
    Expr automatic;
};
}
