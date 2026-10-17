#pragma once

#include <codegen/schema/packed_value_schema.h>
#include <lispb/schema/declaration_id.h>

namespace lispb::schema {
struct ReplacePackedValue {
    DeclarationId declaration;
    codegen::PackedValueSchema schema;
};
}
