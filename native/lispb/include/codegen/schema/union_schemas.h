#pragma once

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indices.h>

#include <vector>

namespace lispb::codegen {

struct UnionSchemas {
    [[nodiscard]] auto append() -> DeclarationHandle;
    [[nodiscard]] auto clone(DeclarationHandle id) -> DeclarationHandle;
    [[nodiscard]] auto index(DeclarationHandle id) const -> std::uint32_t;
    void clear();

    std::vector<IdentifierId> name;
    std::vector<SchemaRange<UnionAlternativeId>> alternatives;
    std::vector<IdentifierId> export_specifier;
};

}
