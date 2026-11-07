#pragma once

#include <codegen/schema/cpp_projection.h>
#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/numeric_formats.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

#include <cstdint>

namespace lispb::codegen {

struct MiniFloatSchemas {
    using Indexing = TaggedIndex<DeclarationHandle, DeclarationKind::mini_float>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append(StringId name,
                              MiniFloatFormat format,
                              std::optional<CppProjection> projection) -> DeclarationHandle;
    [[nodiscard]] auto clone(DeclarationHandle id) -> DeclarationHandle;
    [[nodiscard]] auto index(DeclarationHandle id) const -> std::uint32_t;
    void clear();

    Column<StringId> name;
    Column<std::optional<CppProjection>> cpp_projection;
    Column<MiniFloatFormat> format;
};

}
