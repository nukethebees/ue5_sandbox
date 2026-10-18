#include <lispb/schema/editor_declarations.h>

#include <algorithm>
#include <cassert>

namespace lispb::schema {

auto EditorDeclarations::allocate() -> DeclarationId {
    auto const id{DeclarationId{static_cast<std::uint32_t>(identity.size() + 1)}};
    reserve(id);
    return id;
}
void EditorDeclarations::reserve(DeclarationId const id) {
    assert(lispb::schema::is_valid(id));
    if (id.value <= identity.size()) {
        return;
    }
    identity.resize(id.value);
    module_index.resize(id.value);
    declaration_index.resize(id.value);
    source.resize(id.value);
    active_.resize(id.value);
}
void EditorDeclarations::activate(DeclarationId const id) {
    reserve(id);
    auto const row{index(id)};
    assert(!active_[row]);
    active_[row] = true;
    ordered.push_back(id);
}
void EditorDeclarations::remove(DeclarationId const id) {
    assert(contains(id));
    active_[index(id)] = false;
    std::erase(ordered, id);
}
void EditorDeclarations::clear() {
    ordered.clear();
    identity.clear();
    module_index.clear();
    declaration_index.clear();
    source.clear();
    active_.clear();
}
auto EditorDeclarations::contains(DeclarationId const id) const -> bool {
    return lispb::schema::is_valid(id) && id.value <= active_.size() && active_[id.value - 1] != 0;
}
auto EditorDeclarations::find(DeclarationId const id) const -> std::optional<DeclarationId> {
    return contains(id) ? std::optional{id} : std::nullopt;
}
auto EditorDeclarations::index(DeclarationId const id) const -> std::uint32_t {
    assert(lispb::schema::is_valid(id) && id.value <= identity.size());
    return id.value - 1;
}

}
