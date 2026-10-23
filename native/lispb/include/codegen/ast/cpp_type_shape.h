#pragma once
#include <codegen/ast/cpp_type_form.h>
#include <codegen/ast/ir/range.h>

namespace lispb::codegen {
struct CppTypeShape {
    CppTypeForm form{CppTypeForm::raw};
    ir::Range parts{};
};
}
