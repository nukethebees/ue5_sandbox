#pragma once

#include <codegen/schema/linear_quantized_schema.h>
#include <lispb/schema/declaration_id.h>

namespace lispb::schema {
struct ReplaceLinearQuantized {
    DeclarationId declaration;
    codegen::LinearQuantizedSchema schema;
};
}
