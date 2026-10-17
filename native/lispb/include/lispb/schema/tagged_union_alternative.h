#pragma once

#include <lispb/schema/resolved_type_ref.h>

#include <cstdint>
#include <optional>
#include <string>

namespace lispb::schema {
struct TaggedUnionAlternative {
    std::string name;
    ResolvedTypeRef semantic_type;
    std::optional<std::uint64_t> count;
    std::string tag;
};
}
