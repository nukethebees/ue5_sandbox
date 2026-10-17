#pragma once

#include <codegen/schema/integer_varint_schema.h>
#include <lispb/schema/declaration_id.h>

namespace lispb::schema {
struct ReplaceIntegerVarint {
    DeclarationId declaration;
    codegen::IntegerVarintSchema schema;
};
}
