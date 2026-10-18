#pragma once

#include <codegen/schema/declaration_handle.h>
#include <lispb/schema/type_definition_id.h>

#include <cstdint>
#include <vector>

namespace lispb::schema {

struct StaticTableTypes {
    [[nodiscard]] auto append() -> TypeDefinitionId;
    [[nodiscard]] auto index(TypeDefinitionId id) const -> std::uint32_t;
    std::vector<codegen::DeclarationHandle> source;
};

} // namespace lispb::schema
