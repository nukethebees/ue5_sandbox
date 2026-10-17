#pragma once

#include <codegen/schema/optional_presence_bit_schema.h>
#include <lispb/schema/declaration_id.h>

namespace lispb::schema {
struct ReplaceOptionalPresenceBit {
    DeclarationId declaration;
    codegen::OptionalPresenceBitSchema schema;
};
}
