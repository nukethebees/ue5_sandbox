#include <codegen/schema/setting_schemas.h>

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
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source_index];
    label[destination] = label[source_index];
    tooltip[destination] = tooltip[source_index];
    category[destination] = category[source_index];
    value_type[destination] = value_type[source_index];
    backend[destination] = backend[source_index];
    apply_mode[destination] = apply_mode[source_index];
    device[destination] = device[source_index];
    control[destination] = control[source_index];
    return result;
}
void SettingSchemas::clear() {
    name.clear();
    label.clear();
    tooltip.clear();
    category.clear();
    value_type.clear();
    backend.clear();
    apply_mode.clear();
    device.clear();
    control.clear();
}

}
