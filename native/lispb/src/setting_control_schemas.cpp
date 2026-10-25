#include <codegen/schema/setting_control_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto SettingControlSchemas::index(SettingControlId id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < kind.size());
    return row;
}
auto SettingControlSchemas::append() -> SettingControlId {
    auto const index{static_cast<std::uint32_t>(kind.size())};
    kind.push_back({});
    options_provider.push_back({});
    availability_provider.push_back({});
    minimum.push_back({});
    maximum.push_back({});
    step.push_back({});
    custom_row.push_back({});
    return SettingControlId{index + 1};
}
auto SettingControlSchemas::clone(SettingControlId id) -> SettingControlId {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    kind[destination] = kind[source_index];
    options_provider[destination] = options_provider[source_index];
    availability_provider[destination] = availability_provider[source_index];
    minimum[destination] = minimum[source_index];
    maximum[destination] = maximum[source_index];
    step[destination] = step[source_index];
    custom_row[destination] = custom_row[source_index];
    return result;
}
void SettingControlSchemas::clear() {
    kind.clear();
    options_provider.clear();
    availability_provider.clear();
    minimum.clear();
    maximum.clear();
    step.clear();
    custom_row.clear();
}

}
