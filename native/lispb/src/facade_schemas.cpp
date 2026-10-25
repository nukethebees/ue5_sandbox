#include <codegen/schema/facade_schemas.h>

#include <cassert>

namespace lispb::codegen {

auto FacadeSchemas::index(DeclarationHandle id) const -> std::uint32_t {
    auto const row{Indexing::offset(id)};
    assert(row < name.size());
    return row;
}
auto FacadeSchemas::append() -> DeclarationHandle {
    auto const index{static_cast<std::uint32_t>(name.size())};
    assert(index < 0x00fffffe);
    name.push_back({});
    target_type.push_back({});
    target_member_name.push_back({});
    methods.push_back({});
    validation_lines.push_back({});
    validation_dependencies.push_back({});
    export_specifier.push_back({});
    bind_access.push_back(Access::public_);
    method_access.push_back(Access::public_);
    friends.push_back({});
    friend_kind.push_back(RecordKind::class_);
    definitions_in_source.push_back(false);
    reference_target.push_back(false);
    return DeclarationHandle{(static_cast<std::uint32_t>(DeclarationKind::facade) << 24) |
                             (index + 1)};
}
auto FacadeSchemas::clone(DeclarationHandle id) -> DeclarationHandle {
    auto const source_index{index(id)};
    auto const result{append()};
    auto const destination{index(result)};
    name[destination] = name[source_index];
    target_type[destination] = target_type[source_index];
    target_member_name[destination] = target_member_name[source_index];
    methods[destination] = methods[source_index];
    validation_lines[destination] = validation_lines[source_index];
    validation_dependencies[destination] = validation_dependencies[source_index];
    export_specifier[destination] = export_specifier[source_index];
    bind_access[destination] = bind_access[source_index];
    method_access[destination] = method_access[source_index];
    friends[destination] = friends[source_index];
    friend_kind[destination] = friend_kind[source_index];
    definitions_in_source[destination] = definitions_in_source[source_index];
    reference_target[destination] = reference_target[source_index];
    return result;
}
void FacadeSchemas::clear() {
    name.clear();
    target_type.clear();
    target_member_name.clear();
    methods.clear();
    validation_lines.clear();
    validation_dependencies.clear();
    export_specifier.clear();
    bind_access.clear();
    method_access.clear();
    friends.clear();
    friend_kind.clear();
    definitions_in_source.clear();
    reference_target.clear();
}

}
