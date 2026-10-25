#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/setting_control_kind.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

#include <optional>

namespace lispb::codegen {

struct SettingControlSchemas {
    using Indexing = OneBasedIndex<SettingControlId>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> SettingControlId;
    [[nodiscard]] auto clone(SettingControlId id) -> SettingControlId;
    [[nodiscard]] auto index(SettingControlId id) const -> std::uint32_t;
    void clear();

    Column<SettingControlKind> kind;
    Column<std::optional<StringId>> options_provider;
    Column<std::optional<StringId>> availability_provider;
    Column<std::optional<double>> minimum;
    Column<std::optional<double>> maximum;
    Column<std::optional<double>> step;
    Column<std::optional<StringId>> custom_row;
};

}
