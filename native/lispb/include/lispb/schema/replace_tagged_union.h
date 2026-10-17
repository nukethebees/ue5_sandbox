#pragma once

#include <codegen/schema/tagged_union_schema.h>
#include <lispb/schema/declaration_id.h>

namespace lispb::schema {
struct ReplaceTaggedUnion {
    DeclarationId declaration;
    codegen::TaggedUnionSchema schema;
};
}
