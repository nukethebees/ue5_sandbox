#pragma once

#include <codegen/ast/indices.h>
#include <codegen/ast/syntax.h>
#include <codegen/ast/template_id.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::codegen::ir {

struct Variables {
    std::vector<CppTypeId> type;
    std::vector<Syntax> name;
    std::vector<std::optional<Syntax>> initializer;
    std::vector<TemplateId> template_declaration;
    std::vector<std::uint8_t> is_inline;
    std::vector<std::uint8_t> is_constexpr;
};

} // namespace lispb::codegen::ir
