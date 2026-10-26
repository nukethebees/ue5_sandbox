#pragma once

#include <codegen/schema/cpp_projection.h>
#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/fixed_point_rounding.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>

namespace lispb::codegen {

struct FixedPointSchemas {
    using Indexing = TaggedIndex<DeclarationHandle, DeclarationKind::fixed_point>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> DeclarationHandle;
    [[nodiscard]] auto clone(DeclarationHandle id) -> DeclarationHandle;
    [[nodiscard]] auto index(DeclarationHandle id) const -> std::uint32_t;
    void clear();

    Column<StringId> name;
    Column<std::optional<CppProjection>> cpp_projection;
    Column<std::uint8_t> signedness;
    Column<std::uint32_t> total_bits;
    Column<std::uint32_t> fractional_bits;
    Column<FixedPointRounding> rounding;
    Column<std::optional<StringId>> minimum_value;
    Column<std::optional<StringId>> maximum_value;
};

}
