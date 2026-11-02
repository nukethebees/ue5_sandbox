#pragma once
#include <codegen/ast/cpp_type_form.h>
#include <codegen/ast/expr.h>
#include <codegen/ast/ir/range.h>
#include <codegen/ast/name_origin.h>

namespace lispb::codegen {
struct CppTypeShape {
    CppTypeForm form{CppTypeForm::raw};
    ir::Range parts{};
    NameOrigin origin{NameOrigin::enclosing};
    Expr operand{};
};
}
