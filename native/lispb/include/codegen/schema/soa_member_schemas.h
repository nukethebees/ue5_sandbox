#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/schema_range.h>
#include <codegen/schema/soa_member_kind.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace lispb::codegen {

struct SoaMemberSchemas {
    [[nodiscard]] auto append() -> SoaMemberId;
    [[nodiscard]] auto clone(SoaMemberId id) -> SoaMemberId;
    [[nodiscard]] auto index(SoaMemberId id) const -> std::uint32_t;
    void clear();

    std::vector<IdentifierId> name;
    std::vector<SoaMemberKind> kind;
    std::vector<TypeRefId> type;
    std::vector<std::optional<IdentifierId>> fixed_schema;
    std::vector<std::optional<IdentifierId>> nested_schema;
    std::vector<std::uint8_t> mask_field;
    std::vector<SchemaRange<SoaMaskDimensionId>> mask_dimensions;
    std::vector<std::optional<SemanticRelationId>> relationship;
};

}
