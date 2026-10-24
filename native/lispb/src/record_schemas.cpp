#include <codegen/schema/record_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto RecordSchemas::index(DeclarationHandle id) const -> std::uint32_t {
    assert(id.kind() == DeclarationKind::record);
    auto const row{id.index()};
    assert(row < name.size());
    return row;
}
auto RecordSchemas::append() -> DeclarationHandle {
    auto const index{static_cast<std::uint32_t>(name.size())};
    assert(index < 0x00fffffe);
    name.push_back({});
    generate_settings.push_back(false);
    generate_overrides.push_back(false);
    members.push_back({});
    export_specifier.push_back({});
    comparison.push_back(RecordComparison::none);
    comparison_noexcept.push_back(false);
    functions.push_back({});
    return DeclarationHandle{(static_cast<std::uint32_t>(DeclarationKind::record) << 24) |
                             (index + 1)};
}
auto RecordSchemas::clone(DeclarationHandle id) -> DeclarationHandle {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source_index];
    generate_settings[destination] = generate_settings[source_index];
    generate_overrides[destination] = generate_overrides[source_index];
    members[destination] = members[source_index];
    export_specifier[destination] = export_specifier[source_index];
    comparison[destination] = comparison[source_index];
    comparison_noexcept[destination] = comparison_noexcept[source_index];
    functions[destination] = functions[source_index];
    return result;
}
void RecordSchemas::clear() {
    name.clear();
    generate_settings.clear();
    generate_overrides.clear();
    members.clear();
    export_specifier.clear();
    comparison.clear();
    comparison_noexcept.clear();
    functions.clear();
}

}
