#pragma once

#include <codegen/schema/indices.h>
#include <codegen/schema/setting_apply_mode.h>
#include <codegen/schema/setting_device.h>
#include <lispb/indexed_column.h>
#include <lispb/indices.h>

#include <optional>

namespace lispb::codegen {

struct SettingSchemas {
    using Indexing = OneBasedIndex<SettingId>;
    template <typename T>
    using Column = IndexedColumn<T, Indexing>;

    [[nodiscard]] auto append() -> SettingId;
    [[nodiscard]] auto clone(SettingId id) -> SettingId;
    [[nodiscard]] auto index(SettingId id) const -> std::uint32_t;
    void clear();

    Column<StringId> name;
    Column<StringId> label;
    Column<std::optional<StringId>> tooltip;
    Column<StringId> category;
    Column<TypeRefId> value_type;
    Column<StringId> backend;
    Column<SettingApplyMode> apply_mode;
    Column<SettingDevice> device;
    Column<SettingControlId> control;
};

}
