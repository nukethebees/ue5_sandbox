#pragma once

#include <codegen/schema/soa_schema.h>
#include <lispb/schema/declaration_id.h>

namespace lispb::schema {
struct ReplaceSoa {
    DeclarationId declaration;
    codegen::SoaSchema schema;
};
}
