#include <codegen/schema/type_registry.h>

#include <codegen/ast/cpp_types.h>
#include <codegen/schema.h>
#include <codegen/schema/external_scalar_kind.h>
#include <lispb/strings.h>

#include <cassert>
#include <stdexcept>
#include <string>
#include <utility>

namespace lispb::codegen {

auto TypeRegistry::operator=(TypeRegistry const& other) -> TypeRegistry& {
    assert(&registered == &other.registered);
    entries = other.entries;
    return *this;
}
auto TypeRegistry::operator=(TypeRegistry&& other) noexcept -> TypeRegistry& {
    assert(&registered == &other.registered);
    entries = std::move(other.entries);
    return *this;
}
auto TypeRegistry::find(std::string_view const name) const -> std::optional<RegisteredTypeId> {
    return find(cpp_types.strings().find(name));
}
auto TypeRegistry::find(StringId const name) const -> std::optional<RegisteredTypeId> {
    auto const found{entries.find(name)};
    return found == entries.end() ? std::nullopt : std::optional{found->second};
}
auto TypeRegistry::bind(StringId const name, RegisteredTypeId const type) -> bool {
    return entries.emplace(name, type).second;
}
auto TypeRegistry::add(std::string_view const name,
                       CppTypeId const type,
                       ExternalScalarId const scalar) -> bool {
    return add(cpp_types.strings().intern(name), type, scalar);
}
auto TypeRegistry::add(StringId const name, CppTypeId const type, ExternalScalarId const scalar)
    -> bool {
    if (entries.contains(name)) {
        return false;
    }
    return bind(name, registered.append(type, scalar));
}
void TypeRegistry::clear() {
    entries.clear();
}

auto external_scalar_schema(TypeRegistry const& types, std::string_view const spelling)
    -> std::optional<ExternalScalarId> {
    auto const normalized{native_spelling(std::string{spelling})};
    std::optional<ExternalScalarId> result;
    for (auto const& [name, registered] : types.entries) {
        auto const row{types.registered.index(registered)};
        if (types.registered.semantics[row].kind() == ExternalScalarKind::none ||
            native_spelling(std::string{
                types.cpp_types.spelling(types.registered.cpp_type[row])}) != normalized) {
            continue;
        }
        if (result && *result != types.registered.semantics[row]) {
            throw std::invalid_argument{"Conflicting scalar descriptions for C++ type '" +
                                        std::string{spelling} + "'"};
        }
        result = types.registered.semantics[row];
    }
    return result;
}

} // namespace lispb::codegen
