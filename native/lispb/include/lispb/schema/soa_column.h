#pragma once

#include <codegen/schema/soa_member_kind.h>
#include <lispb/schema/resolved_type_ref.h>
#include <lispb/schema/semantic_relationship.h>
#include <lispb/schema/type_id.h>

#include <optional>
#include <string>

namespace lispb::schema {
struct SoaColumn {
    std::string name;
    ResolvedTypeRef semantic_type;
    codegen::SoaMemberKind kind{codegen::SoaMemberKind::array};
    std::optional<TypeId> nested_type;
    std::optional<SemanticRelationship> relationship;
};
}
