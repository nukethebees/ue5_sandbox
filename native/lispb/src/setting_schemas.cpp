#include <codegen/schema/setting_schemas.h>

#include "schema_columns.h"

#include <cassert>

namespace lispb::codegen {

auto SettingSchemas::index(SettingId id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto SettingSchemas::append(SettingControl value) -> SettingId {
    auto const index{static_cast<std::uint32_t>(name.size())};
    name.push_back({});
    label.push_back({});
    tooltip.push_back({});
    category.push_back({});
    value_type.push_back({});
    backend.push_back({});
    apply_mode.push_back({});
    device.push_back(SettingDevice::shared);
    control.push_back(value);
    return SettingId{index + 1};
}
auto SettingSchemas::clone(SettingId id) -> SettingId {
    auto const result{append(control[id])};
    name[result] = name[id];
    label[result] = label[id];
    tooltip[result] = tooltip[id];
    category[result] = category[id];
    value_type[result] = value_type[id];
    backend[result] = backend[id];
    apply_mode[result] = apply_mode[id];
    device[result] = device[id];
    return result;
}
void SettingSchemas::clear() {
    detail::clear_schema_columns(
        name, label, tooltip, category, value_type, backend, apply_mode, device, control);
}

}
