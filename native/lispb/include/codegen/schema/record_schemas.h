#pragma once

#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/record_comparison.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indices.h>

#include <cstdint>
#include <vector>

namespace lispb::codegen {

struct RecordSchemas {
    [[nodiscard]] auto append() -> DeclarationHandle;
    [[nodiscard]] auto clone(DeclarationHandle id) -> DeclarationHandle;
    [[nodiscard]] auto index(DeclarationHandle id) const -> std::uint32_t;
    void clear();

    std::vector<IdentifierId> name;
    std::vector<SchemaRange<RecordMemberId>> members;
    std::vector<IdentifierId> export_specifier;
    std::vector<RecordComparison> comparison;
    std::vector<std::uint8_t> comparison_noexcept;
    std::vector<SchemaRange<FunctionSchemaId>> functions;
};

}
