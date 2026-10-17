#pragma once

#include <codegen/schema/integer_varint_schema.h>
#include <lispb/schema/resolved_type_ref.h>

namespace lispb::schema {
struct IntegerVarintType {
    ResolvedTypeRef source;
    codegen::IntegerVarintEncoding encoding{codegen::IntegerVarintEncoding::unsigned_varint};
};
}
