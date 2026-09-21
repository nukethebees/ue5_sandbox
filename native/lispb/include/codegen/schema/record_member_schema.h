#pragma once

#include <codegen/schema/semantic_relation_schema.h>
#include <codegen/schema/type_ref.h>

#include <cstdint>
#include <optional>
#include <string>

namespace codegen {

struct RecordMemberSchema {
    std::string name;
    TypeRef type;
    std::optional<std::uint64_t> count;
    std::optional<SemanticRelationSchema> relationship;
};

} // namespace codegen
