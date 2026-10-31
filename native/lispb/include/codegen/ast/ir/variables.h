#pragma once

#include <codegen/ast/expr.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/template_id.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::codegen::ir {

struct Variables {
    std::vector<CppTypeId> type;
    std::vector<Expr> name;
    std::vector<std::optional<Expr>> initializer;
    std::vector<TemplateId> template_declaration;
    std::vector<std::uint8_t> is_inline;
    std::vector<std::uint8_t> is_constexpr;
};

} // namespace lispb::codegen::ir
