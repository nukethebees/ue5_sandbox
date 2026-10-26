#pragma once

#include <codegen/schema/cpp_projection.h>
#include <codegen/schema/declaration_handle.h>
#include <codegen/schema/declaration_kind.h>
#include <codegen/schema/indices.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

namespace lispb::codegen {

struct OptionalSentinelSchemas {
    using Indexing = TaggedIndex<DeclarationHandle, DeclarationKind::optional_sentinel>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> DeclarationHandle;
    [[nodiscard]] auto clone(DeclarationHandle id) -> DeclarationHandle;
    [[nodiscard]] auto index(DeclarationHandle id) const -> std::uint32_t;
    void clear();

    Column<StringId> name;
    Column<std::optional<CppProjection>> cpp_projection;
    Column<TypeRefId> source;
    Column<StringId> sentinel;
};

}
