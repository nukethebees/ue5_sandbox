#pragma once
#include <codegen/ast/indices.h>
#include <lispb/indices.h>

namespace lispb::codegen {
// Compose generated type syntax without flattening references to other types.
struct CppTypePart {
    StringId text{};
    CppTypeId type{};

    auto operator==(CppTypePart const&) const -> bool = default;
};
}
