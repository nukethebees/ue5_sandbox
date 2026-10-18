#include <ioj/layout/editor_session.hpp>

#include <lispb/diagnostic_severity.h>

namespace ioj::layout {

auto EditorSession::storage() -> lispb::CompilerStorage& {
    return storage_;
}
auto EditorSession::diagnostics() const -> lispb::Diagnostics const& {
    return storage_.diagnostics;
}
auto EditorSession::format_diagnostic(lispb::DiagnosticId diagnostic) const -> std::string {
    return storage_.diagnostics.format(diagnostic, storage_.sources);
}
auto EditorSession::last_diagnostic_begin() const -> std::size_t {
    return diagnostic_begin_;
}
void EditorSession::begin_operation() {
    diagnostic_begin_ = storage_.diagnostics.size();
}
void EditorSession::record_error(std::string_view message) {
    storage_.diagnostics.add(lispb::DiagnosticSeverity::error, {}, message);
}
auto EditorSession::allocate_document_id() -> DocumentId {
    auto const id{next_document_id_};
    ++next_document_id_.value;
    return id;
}

}
