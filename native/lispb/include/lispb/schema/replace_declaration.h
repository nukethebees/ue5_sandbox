#pragma once

#include <codegen/schema/declaration_schema.h>
#include <lispb/schema/declaration_id.h>

namespace lispb::schema {
struct ReplaceDeclaration {
    DeclarationId declaration;
    codegen::DeclarationSchema schema;
};
}
