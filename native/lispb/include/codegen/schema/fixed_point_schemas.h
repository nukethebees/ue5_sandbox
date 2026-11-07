#pragma once

#include <codegen/schema/cpp_projection.h>
#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/fixed_point_form.h>
#include <codegen/schema/fixed_point_rounding.h>
#include <codegen/schema/numeric_formats.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>

namespace lispb::codegen {

struct FixedPointSchemas {
    using Indexing = TaggedIndex<DeclarationHandle, DeclarationKind::fixed_point>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append(StringId name,
                              FixedPointFormat format,
                              std::optional<CppProjection> projection,
                              FixedPointRounding rounding) -> DeclarationHandle;
    [[nodiscard]] auto clone(DeclarationHandle id) -> DeclarationHandle;
    [[nodiscard]] auto index(DeclarationHandle id) const -> std::uint32_t;
    void clear();

    Column<StringId> name;
    Column<std::optional<CppProjection>> cpp_projection;
    Column<FixedPointFormat> format;
    Column<FixedPointRounding> rounding;
};

}
