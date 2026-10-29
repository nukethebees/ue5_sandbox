#include <codegen/schema/facade_schemas.h>

#include "schema_columns.h"

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
    return detail::clone_schema_row(*this,
                                    id,
                                    name,
                                    target_type,
                                    target_member_name,
                                    methods,
                                    validation_lines,
                                    validation_dependencies,
                                    export_specifier,
                                    bind_access,
                                    method_access,
                                    friends,
                                    friend_kind,
                                    definitions_in_source,
                                    reference_target);
}
void FacadeSchemas::clear() {
    detail::clear_schema_columns(name,
                                 target_type,
                                 target_member_name,
                                 methods,
                                 validation_lines,
                                 validation_dependencies,
                                 export_specifier,
                                 bind_access,
                                 method_access,
                                 friends,
                                 friend_kind,
                                 definitions_in_source,
                                 reference_target);
}

}
