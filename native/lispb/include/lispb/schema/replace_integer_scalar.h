#pragma once

#include <codegen/schema/integer_scalar_schema.h>
#include <lispb/schema/declaration_id.h>

namespace lispb::schema {
struct ReplaceIntegerScalar {
    DeclarationId declaration;
    codegen::IntegerScalarSchema schema;
};
}
