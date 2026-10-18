#include "document_state.hpp"

#include <lispb/diagnostic_severity.h>

namespace ioj::layout {
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
void DocumentState::error(std::string_view message) {
    storage.diagnostics.add(lispb::DiagnosticSeverity::error, {}, message);
}
}
