#pragma once

#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/type_ref.h>

#include <optional>
#include <string>

namespace codegen {

struct SoaMemberSchema {
    std::string name;
    SoaMemberKind kind;
    TypeRef type;
    std::optional<std::string> fixed_schema;
};

} // namespace codegen
