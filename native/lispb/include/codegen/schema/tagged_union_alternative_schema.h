#pragma once

#include <codegen/schema/type_ref.h>

#include <cstdint>
#include <optional>
#include <string>

namespace codegen {

struct TaggedUnionAlternativeSchema {
    std::string name;
    TypeRef type;
    std::optional<std::uint64_t> count;
    std::string tag;
};

} // namespace codegen
