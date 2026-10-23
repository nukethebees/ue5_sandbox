#pragma once

#include <codegen/schema/type_reference_category.h>
#include <codegen/schema/type_reference_part.h>
#include <lispb/indices.h>

#include <compare>
#include <cstdint>
#include <string>

namespace lispb {
class Strings;
}

namespace lispb::codegen {

struct TypeReferenceRole {
    TypeReferenceCategory category{};
    TypeReferencePart part{TypeReferencePart::direct};
    StringId subject;
    StringId parameter;
    std::uint32_t index{};
    std::uint32_t secondary_index{};

    auto operator<=>(TypeReferenceRole const&) const = default;
};

auto type_reference_role_text(Strings const& strings, TypeReferenceRole const& role) -> std::string;

} // namespace lispb::codegen
