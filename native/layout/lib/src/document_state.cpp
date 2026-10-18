#include <ioj/layout/document_state.hpp>

#include <ioj/layout/editor_session.hpp>

#include <utility>

namespace ioj::layout {
DocumentState::DocumentState(EditorSession& owner, DocumentId identity)
    : session{&owner}
    , id{identity}
    , history{owner, identity} {}
auto DocumentState::register_declaration(lispb::codegen::DeclarationHandle declaration)
    -> lispb::schema::DeclarationId {
    if (auto const found{declaration_ids.find(declaration)}; found != declaration_ids.end()) {
        return found->second;
    }
    auto const [found, inserted]{creation_ids.emplace(
        std::pair{current_instruction_id, creation_slot++}, next_declaration_id)};
    if (inserted) {
        ++next_declaration_id.value;
    }
    declaration_ids.emplace(declaration, found->second);
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
    return session->storage(id);
}
void DocumentState::reset_workspace() {
    session->capture_diagnostics(id);
    types = {};
    modules.clear();
    registered_types.clear();
    comments = {};
    variants = {};
    analysis_settings = {};
    declaration_ids.clear();
    source_files.clear();
    source_order.clear();
    source_ids.clear();
    render_origins.clear();
    current_instruction_id = {};
    creation_slot = 0;
    source_creation_slot = 0;
    changed = false;
    session->reset_storage(id);
}
auto DocumentState::manifest() const -> lispb::codegen::Manifest {
    auto& stores{storage()};
    lispb::codegen::TypeRegistry registry{stores.registered_types};
    registry.entries = registered_types;
    return {
        lispb::codegen::manifest_schema_version, std::move(registry), stores.declarations, modules};
}
}
