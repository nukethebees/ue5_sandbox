#include <codegen/schema/setting_control_schemas.h>

#include "schema_columns.h"

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
    return detail::clone_schema_row(*this,
                                    id,
                                    kind,
                                    options_provider,
                                    availability_provider,
                                    minimum,
                                    maximum,
                                    step,
                                    custom_row);
}
void SettingControlSchemas::clear() {
    detail::clear_schema_columns(
        kind, options_provider, availability_provider, minimum, maximum, step, custom_row);
}

}
