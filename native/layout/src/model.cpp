#include <ioj/layout/model.hpp>

#include <algorithm>

namespace ioj::layout {
namespace {

auto id_of(LayoutDefinition const& definition) -> SchemaId const& {
    return std::visit([](auto const& layout) -> SchemaId const& { return layout.id; }, definition);
}

} // namespace

auto SchemaCatalog::add(LayoutDefinition definition) -> bool {
    auto const id{id_of(definition)};
    if (find(id) != nullptr) {
        return false;
    }
    items_.push_back(std::move(definition));
    return true;
}

auto SchemaCatalog::find(SchemaId const& id) const -> LayoutDefinition const* {
    auto const found{
        std::ranges::find_if(items_, [&](auto const& item) { return id_of(item) == id; })};
    return found == items_.end() ? nullptr : &*found;
}

auto SchemaCatalog::items() const -> std::vector<LayoutDefinition> const& {
    return items_;
}

} // namespace ioj::layout
