#pragma once

#include <ioj/levels/authoring/expr_input.h>
#include <ioj/levels/authoring/identifier_writer.h>
#include <ioj/levels/objective_dependency_writer.h>
#include <ioj/settings/settings_values_writer.h>

namespace ioj::s7 {
template <>
struct TypeWriter<levels::authoring::ExprInput> {
    static auto write(SExprIR& ir,
                      levels::authoring::ExprInput const& value,
                      levels::authoring::ExprInput const*) -> Expr;
    static auto equivalent(levels::authoring::ExprInput const& value,
                           levels::authoring::ExprInput const& base) -> bool;
};
}
