#pragma once

#include <codegen/schema/type_ref.h>

#include <optional>
#include <string_view>

namespace codegen {

enum class SemanticRelationKind {
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

enum class SemanticRelationUnit { elements, bytes };

inline auto semantic_relation_kind_name(SemanticRelationKind const kind) -> std::string_view {
    switch (kind) {
        case SemanticRelationKind::index_into:
            return "index_into";
        case SemanticRelationKind::count_of:
            return "count_of";
        case SemanticRelationKind::offset_into:
            return "offset_into";
        case SemanticRelationKind::discriminates:
            return "discriminates";
        case SemanticRelationKind::contains:
            return "contains";
        case SemanticRelationKind::member_of:
            return "member_of";
        case SemanticRelationKind::quantises:
            return "quantises";
        case SemanticRelationKind::encoded_as:
            return "encoded_as";
        case SemanticRelationKind::references:
            return "references";
    }
    return "references";
}

inline auto semantic_relation_unit_name(SemanticRelationUnit const unit) -> std::string_view {
    switch (unit) {
        case SemanticRelationUnit::elements:
            return "elements";
        case SemanticRelationUnit::bytes:
            return "bytes";
    }
    return "elements";
}

struct SemanticRelationSchema {
    SemanticRelationKind kind{SemanticRelationKind::references};
    TypeRef target;
    std::optional<SemanticRelationUnit> unit;
};

} // namespace codegen
