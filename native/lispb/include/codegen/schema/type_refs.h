#pragma once

#include <codegen/schema/indices.h>
#include <codegen/sexpr/indices.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

#include <optional>

namespace lispb::codegen {

struct TypeRefs {
    using Indexing = OneBasedIndex<TypeRefId>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> TypeRefId;
    [[nodiscard]] auto clone(TypeRefId id) -> TypeRefId;
    [[nodiscard]] auto index(TypeRefId id) const -> std::uint32_t;
    void clear();

    Column<StringId> name;
    Column<sexpr::FormId> authored_name;
    Column<StringId> suffix;
    Column<std::optional<StringId>> nested;
};

}
