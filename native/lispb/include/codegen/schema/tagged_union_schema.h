#pragma once

#include <codegen/schema/tagged_union_alternative_schema.h>
#include <codegen/schema/type_ref.h>

#include <optional>
#include <string>
#include <vector>

namespace codegen {

struct TaggedUnionSchema {
    std::string name;
    TypeRef discriminant;
    std::vector<TaggedUnionAlternativeSchema> alternatives;
    std::optional<std::string> export_specifier;
};

} // namespace codegen
