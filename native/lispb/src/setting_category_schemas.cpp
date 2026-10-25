#include <codegen/schema/setting_category_schemas.h>

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
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source_index];
    label[destination] = label[source_index];
    return result;
}
void SettingCategorySchemas::clear() {
    name.clear();
    label.clear();
}

}
