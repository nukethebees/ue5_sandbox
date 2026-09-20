#pragma once

#include <codegen/schema/type_ref.h>

#include <string_view>

namespace codegen {

enum class PackedFieldRelationKind {
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

inline auto packed_field_relation_kind_name(PackedFieldRelationKind const kind)
    -> std::string_view {
    switch (kind) {
        case PackedFieldRelationKind::index_into:
            return "index_into";
        case PackedFieldRelationKind::count_of:
            return "count_of";
        case PackedFieldRelationKind::offset_into:
            return "offset_into";
        case PackedFieldRelationKind::discriminates:
            return "discriminates";
        case PackedFieldRelationKind::contains:
            return "contains";
        case PackedFieldRelationKind::member_of:
            return "member_of";
        case PackedFieldRelationKind::quantises:
            return "quantises";
        case PackedFieldRelationKind::encoded_as:
            return "encoded_as";
        case PackedFieldRelationKind::references:
            return "references";
    }
    return "references";
}

struct PackedFieldRelationSchema {
    PackedFieldRelationKind kind{PackedFieldRelationKind::references};
    TypeRef target;
};

} // namespace codegen
