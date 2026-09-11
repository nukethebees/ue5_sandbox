#pragma once

#include <codegen/ast/cpp_type.h>
#include <codegen/ast/function_disposition.h>

#include <optional>
#include <string>

namespace codegen {

enum class RefQualifier { none, lvalue, rvalue };

struct FunctionQualifiers {
    std::optional<CppType> trailing_return_type;
    std::optional<std::string> noexcept_condition;
    bool is_const{false};
    bool is_noexcept{false};
    FunctionDisposition disposition{FunctionDisposition::normal};
    RefQualifier ref_qualifier{RefQualifier::none};
};

} // namespace codegen
