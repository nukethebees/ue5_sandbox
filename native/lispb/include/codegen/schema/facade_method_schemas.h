#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/schema_range.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

#include <cstdint>
#include <optional>

namespace lispb::codegen {

struct FacadeMethodSchemas {
    using Indexing = OneBasedIndex<FacadeMethodId>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> FacadeMethodId;
    [[nodiscard]] auto clone(FacadeMethodId id) -> FacadeMethodId;
    [[nodiscard]] auto index(FacadeMethodId id) const -> std::uint32_t;
    void clear();

    Column<StringId> name;
    Column<TypeRefId> return_type;
    Column<SchemaRange<ParameterId>> parameters;
    Column<std::uint8_t> is_const;
    Column<std::uint8_t> is_noexcept;
    Column<std::optional<StringId>> target_name;
};

}
