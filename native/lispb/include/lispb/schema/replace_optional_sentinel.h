#pragma once

#include <codegen/schema/optional_sentinel_schema.h>
#include <lispb/schema/declaration_id.h>

namespace lispb::schema {
struct ReplaceOptionalSentinel {
    DeclarationId declaration;
    codegen::OptionalSentinelSchema schema;
};
}
