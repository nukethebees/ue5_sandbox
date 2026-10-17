#pragma once

#include <codegen/schema/record_schema.h>
#include <lispb/schema/declaration_id.h>

namespace lispb::schema {
struct ReplaceRecord {
    DeclarationId declaration;
    codegen::RecordSchema schema;
};
}
