#pragma once
#include <codegen/ast/expr.h>
#include <codegen/ast/indices.h>
#include <lispb/indices.h>

#include <cstdint>
#include <vector>
namespace lispb::codegen::ir {
struct ValueTemplateParameters {
    std::vector<CppTypeId> type;
    std::vector<StringId> name;
    std::vector<Expr> default_value;
    std::vector<std::uint8_t> pack;
};
}
