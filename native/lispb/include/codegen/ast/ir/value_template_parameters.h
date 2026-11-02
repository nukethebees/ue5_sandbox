#pragma once
#include <codegen/ast/indices.h>
#include <codegen/ast/syntax.h>
#include <lispb/indices.h>

#include <cstdint>
#include <vector>
namespace lispb::codegen::ir {
struct ValueTemplateParameters {
    std::vector<CppTypeId> type;
    std::vector<StringId> name;
    std::vector<Syntax> default_value;
    std::vector<std::uint8_t> pack;
};
}
