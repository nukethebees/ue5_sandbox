#include <codegen/schema/settings_schema_schemas.h>

#include <ioj/ascii.h>

#include <cassert>

namespace lispb::codegen {
auto SettingsSchemaSchemas::index(DeclarationHandle const id) const -> RowIndex {
    assert(id.kind() == DeclarationKind::settings_schema);
    auto const row{id.index()};
    assert(row < name.size());
    return row;
}

auto SettingsSchemaSchemas::append() -> DeclarationHandle {
    auto const row{static_cast<RowIndex>(name.size())};
    assert(row < 0x00fffffe);
    name.push_back({});
    records.push_back({});
    target.push_back({});
    target_header.push_back({});
    paths.push_back({});
    members.push_back({});
    return DeclarationHandle{(static_cast<std::uint32_t>(DeclarationKind::settings_schema) << 24) |
                             (row + 1)};
}

auto SettingsSchemaSchemas::clone(DeclarationHandle const id) -> DeclarationHandle {
    auto const source{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source];
    records[destination] = records[source];
    target[destination] = target[source];
    target_header[destination] = target_header[source];
    paths[destination] = paths[source];
    members[destination] = members[source];
    return result;
}

void SettingsSchemaSchemas::clear() {
    name.clear();
    records.clear();
    target.clear();
    target_header.clear();
    paths.clear();
    members.clear();
}

auto settings_schema_function_stem(std::string_view const name) -> std::string {
    std::string result;
    auto const count{name.size()};
    for (std::size_t index{}; index < count; ++index) {
        auto const character{name[index]};
        auto const uppercase{ioj::isupper(character)};
        auto const previous_lower{index > 0 && ioj::islower(name[index - 1])};
        auto const next_lower{index + 1 < count && ioj::islower(name[index + 1])};
        if (uppercase && index > 0 && (previous_lower || next_lower)) {
            result += '_';
        }
        result += ioj::tolower(character);
    }
    return result;
}

auto settings_schema_field_name(std::string_view const path) -> std::string {
    std::string result;
    bool uppercase{true};
    for (auto const character : path) {
        if (character == '.' || character == '-' || character == '_') {
            uppercase = true;
        } else {
            result += uppercase ? ioj::toupper(character) : character;
            uppercase = false;
        }
    }
    return result;
}
}
