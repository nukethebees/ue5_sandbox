#pragma once

#include <codegen/schema/union_schema.h>
#include <lispb/schema/declaration_id.h>

namespace lispb::schema {
struct ReplaceUnion {
    DeclarationId declaration;
    codegen::UnionSchema schema;
};
}
