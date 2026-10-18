#pragma once

#include <cstdint>
#include <string_view>

namespace lispb::codegen {

enum class SemanticRelationUnit : std::uint8_t { elements, bytes };

auto semantic_relation_unit_name(SemanticRelationUnit const unit) -> std::string_view;

} // namespace lispb::codegen
