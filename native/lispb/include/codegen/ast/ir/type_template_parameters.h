#pragma once
#include <codegen/ast/indices.h>
#include <lispb/indices.h>

#include <cstdint>
#include <vector>
namespace lispb::codegen::ir {
struct TypeTemplateParameters {
    std::vector<StringId> name;
    std::vector<CppTypeId> default_type;
    std::vector<std::uint8_t> pack;
};
}
