#pragma once

#include <codegen/schema/semantic_relation_schema.h>
#include <lispb/schema/resolved_type_ref.h>

#include <optional>

namespace lispb::schema {
struct SemanticRelationship {
    codegen::SemanticRelationKind kind{codegen::SemanticRelationKind::references};
    ResolvedTypeRef target;
    std::optional<codegen::SemanticRelationUnit> unit;
};
}
