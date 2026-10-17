#pragma once

#include <lispb/schema/resolved_type_ref.h>
#include <lispb/schema/semantic_relationship.h>

#include <cstdint>
#include <optional>
#include <string>

namespace lispb::schema {
struct RecordMember {
    std::string name;
    ResolvedTypeRef semantic_type;
    std::optional<std::uint64_t> count;
    std::optional<SemanticRelationship> relationship;
    std::optional<std::string> initializer{};
};
}
