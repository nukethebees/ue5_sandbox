#include <ioj/layout/document_state.hpp>

#include <ioj/layout/editor_session.hpp>

#include <utility>

namespace ioj::layout {
DocumentState::DocumentState(EditorSession& owner, DocumentId identity)
    : session{&owner}
    , id{identity} {}
auto DocumentState::register_declaration(lispb::codegen::DeclarationHandle declaration)
    -> lispb::schema::DeclarationId {
    auto const [found, inserted]{declaration_ids.emplace(declaration, next_declaration_id)};
    if (inserted) {
        ++next_declaration_id.value;
    }
    return found->second;
}
void DocumentState::inherit_declaration(lispb::codegen::DeclarationHandle before,
                                        lispb::codegen::DeclarationHandle after) {
    declaration_ids.emplace(after, declaration_ids.at(before));
    comments.inherit(before, after);
}
void DocumentState::inherit_module(lispb::codegen::ModuleHandle before,
                                   lispb::codegen::ModuleHandle after) {
    source_files.emplace(after, source_files.at(before));
    comments.inherit(before, after);
}
auto DocumentState::storage() const -> lispb::CompilerStorage& {
    return session->storage();
}
auto DocumentState::manifest() const -> lispb::codegen::Manifest {
    auto& stores{storage()};
    lispb::codegen::TypeRegistry registry{stores.registered_types};
    registry.entries = registered_types;
    return {
        lispb::codegen::manifest_schema_version, std::move(registry), stores.declarations, modules};
}
}
