#pragma once

#include <codegen/schema/indices.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

namespace lispb::codegen {

struct SettingCategorySchemas {
    using Indexing = OneBasedIndex<SettingCategoryId>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> SettingCategoryId;
    [[nodiscard]] auto clone(SettingCategoryId id) -> SettingCategoryId;
    [[nodiscard]] auto index(SettingCategoryId id) const -> std::uint32_t;
    void clear();

    Column<StringId> name;
    Column<StringId> label;
};

}
