#pragma once

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::codegen {

struct VectorSoaSchemas {
    [[nodiscard]] auto append() -> DeclarationHandle;
    [[nodiscard]] auto clone(DeclarationHandle id) -> DeclarationHandle;
    [[nodiscard]] auto index(DeclarationHandle id) const -> std::uint32_t;
    void clear();

    std::vector<IdentifierId> name;
    std::vector<TypeRefId> value_type;
    std::vector<SchemaRange<IdentifierId>> components;
    std::vector<SchemaRange<IdentifierId>> equivalent_members;
    std::vector<std::optional<StringId>> equivalent_constructor;
    std::vector<TypeRefId> equivalent_type;
    std::vector<IdentifierId> export_specifier;
    std::vector<std::optional<FixedSoaId>> fixed;
    std::vector<std::uint8_t> equivalent_accessors;
};

}
