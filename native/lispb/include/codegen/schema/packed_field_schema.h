#pragma once

#include <codegen/schema/packed_field_kind.h>
#include <codegen/schema/type_ref.h>

#include <string>

namespace codegen {

struct PackedFieldSchema {
    std::string name;
    TypeRef type;
    int bits;
    PackedFieldKind kind{PackedFieldKind::unsigned_integer};
};

} // namespace codegen
