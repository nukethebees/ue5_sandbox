#pragma once

#include <codegen/schema/fixed_point_schema.h>
#include <lispb/schema/declaration_id.h>

namespace lispb::schema {
struct ReplaceFixedPoint {
    DeclarationId declaration;
    codegen::FixedPointSchema schema;
};
}
