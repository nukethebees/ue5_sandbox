#pragma once
#include <codegen/ast/cpp_type_form.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/ir/range.h>
#include <codegen/ast/name_origin.h>
#include <codegen/ast/syntax.h>
#include <lispb/indices.h>

namespace lispb::codegen {
struct CppTypeShape {
    CppTypeForm form{CppTypeForm::raw};
    ir::Range parts{};
    NameOrigin origin{NameOrigin::enclosing};
    Syntax operand{};
    CppTypeId owner{};
    StringId member{};
};
}
