#pragma once

#include <codegen/ast/indices.h>
#include <codegen/ast/syntax.h>
#include <codegen/ast/template_id.h>
#include <lispb/indices.h>

#include <cstdint>
#include <vector>

namespace lispb::codegen::ir {

struct StaticMembers {
    std::vector<CppTypeId> type;
    std::vector<StringId> name;
    std::vector<Syntax> initializer;
    std::vector<TemplateId> template_declaration;
    std::vector<std::uint8_t> is_inline;
};

}
