#include <codegen/schema/setting_schemas.h>

#include "schema_columns.h"

#include <cassert>

namespace lispb::codegen {

auto SettingSchemas::index(SettingId id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto SettingSchemas::append() -> SettingId {
    auto const index{static_cast<std::uint32_t>(name.size())};
    name.push_back({});
    label.push_back({});
    tooltip.push_back({});
    category.push_back({});
    value_type.push_back({});
    backend.push_back({});
    apply_mode.push_back({});
    device.push_back(SettingDevice::shared);
    control.push_back({});
    return SettingId{index + 1};
}
auto SettingSchemas::clone(SettingId id) -> SettingId {
    return detail::clone_schema_row(*this,
                                    id,
                                    name,
                                    label,
                                    tooltip,
                                    category,
                                    value_type,
                                    backend,
                                    apply_mode,
                                    device,
                                    control);
}
void SettingSchemas::clear() {
    detail::clear_schema_columns(
        name, label, tooltip, category, value_type, backend, apply_mode, device, control);
}

}
