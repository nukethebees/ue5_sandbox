#pragma once

#include <codegen/schema/mini_float_schema.h>
#include <lispb/schema/declaration_id.h>

namespace lispb::schema {
struct ReplaceMiniFloat {
    DeclarationId declaration;
    codegen::MiniFloatSchema schema;
};
}
