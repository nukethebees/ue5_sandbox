#pragma once

#include <codegen/schema/indices.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

#include <optional>

namespace lispb::codegen {

struct ParameterSchemas {
    using Indexing = OneBasedIndex<ParameterId>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> ParameterId;
    [[nodiscard]] auto clone(ParameterId id) -> ParameterId;
    [[nodiscard]] auto index(ParameterId id) const -> std::uint32_t;
    void clear();

    Column<TypeRefId> type;
    Column<StringId> name;
    Column<std::optional<StringId>> default_value;
};

}
