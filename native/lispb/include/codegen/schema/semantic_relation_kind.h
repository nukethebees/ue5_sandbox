#pragma once

#include <cstdint>
#include <string_view>

namespace lispb::codegen {

enum class SemanticRelationKind : std::uint8_t {
    index_into,
    count_of,
    offset_into,
    discriminates,
    contains,
    member_of,
    quantises,
    encoded_as,
    references,
};

auto semantic_relation_kind_name(SemanticRelationKind const kind) -> std::string_view;

} // namespace lispb::codegen
