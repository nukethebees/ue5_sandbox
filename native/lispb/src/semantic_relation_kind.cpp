#include <codegen/schema/semantic_relation_kind.h>

#include <string_view>

namespace lispb::codegen {

auto semantic_relation_kind_name(SemanticRelationKind const kind) -> std::string_view {
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

} // namespace lispb::codegen
