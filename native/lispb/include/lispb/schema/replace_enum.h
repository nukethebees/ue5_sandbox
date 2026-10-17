#pragma once

#include <codegen/schema/enum_schema.h>
#include <lispb/schema/declaration_id.h>

namespace lispb::schema {
struct ReplaceEnum {
    DeclarationId declaration;
    codegen::EnumSchema schema;
};
}
