#include <codegen/schema/setting_category_schemas.h>

#include "schema_columns.h"

#include <cassert>

namespace lispb::codegen {

auto SettingCategorySchemas::index(SettingCategoryId id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto SettingCategorySchemas::append() -> SettingCategoryId {
    auto const index{static_cast<std::uint32_t>(name.size())};
    name.push_back({});
    label.push_back({});
    return SettingCategoryId{index + 1};
}
auto SettingCategorySchemas::clone(SettingCategoryId id) -> SettingCategoryId {
    return detail::clone_schema_row(*this, id, name, label);
}
void SettingCategorySchemas::clear() {
    detail::clear_schema_columns(name, label);
}

}
