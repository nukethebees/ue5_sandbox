#include <ioj/layout/editor_diagnostics.hpp>

#include <lispb/strings.h>

#include <cassert>
#include <limits>

namespace ioj::layout {

EditorDiagnostics::EditorDiagnostics(lispb::Strings& strings)
    : strings_{strings} {}
auto EditorDiagnostics::add(DocumentId const document,
                            OperationId const operation,
                            std::optional<InstructionId> const instruction,
                            lispb::DiagnosticSeverity const severity,
                            std::string_view const message,
                            std::string_view const path,
                            std::uint32_t const line,
                            std::uint32_t const column) -> lispb::DiagnosticId {
    assert(messages_.size() < std::numeric_limits<std::uint32_t>::max());
    auto const id{lispb::DiagnosticId{static_cast<std::uint32_t>(messages_.size())}};

    // Both arguments may borrow the shared pool that appending text can reallocate.
    std::string const owned_message{message};
    std::string const owned_path{path};
    messages_.push_back(strings_.intern_retained(owned_message));
    paths_.push_back(strings_.intern_retained(owned_path));
    severities_.push_back(severity);
    lines_.push_back(line);
    columns_.push_back(column);
    documents_.push_back(document);
    operations_.push_back(operation);
    instructions_.push_back(instruction);
    return id;
}
auto EditorDiagnostics::size() const -> std::size_t {
    return messages_.size();
}
auto EditorDiagnostics::severity(lispb::DiagnosticId const id) const -> lispb::DiagnosticSeverity {
    assert(id.value < size());
    return severities_[id.value];
}
auto EditorDiagnostics::message(lispb::DiagnosticId const id) const -> std::string_view {
    assert(id.value < size());
    return strings_.view(messages_[id.value]);
}
auto EditorDiagnostics::path(lispb::DiagnosticId const id) const -> std::string_view {
    assert(id.value < size());
    return strings_.view(paths_[id.value]);
}
auto EditorDiagnostics::line(lispb::DiagnosticId const id) const -> std::uint32_t {
    assert(id.value < size());
    return lines_[id.value];
}
auto EditorDiagnostics::column(lispb::DiagnosticId const id) const -> std::uint32_t {
    assert(id.value < size());
    return columns_[id.value];
}
auto EditorDiagnostics::document(lispb::DiagnosticId const id) const -> DocumentId {
    assert(id.value < size());
    return documents_[id.value];
}
auto EditorDiagnostics::operation(lispb::DiagnosticId const id) const -> OperationId {
    assert(id.value < size());
    return operations_[id.value];
}
auto EditorDiagnostics::instruction(lispb::DiagnosticId const id) const
    -> std::optional<InstructionId> {
    assert(id.value < size());
    return instructions_[id.value];
}
auto EditorDiagnostics::format(lispb::DiagnosticId const id) const -> std::string {
    std::string result;
    if (!path(id).empty()) {
        result = std::string{path(id)} + ':' + std::to_string(line(id)) + ':' +
                 std::to_string(column(id)) + ": ";
    }
    switch (severity(id)) {
        case lispb::DiagnosticSeverity::note:
            result += "note: ";
            break;
        case lispb::DiagnosticSeverity::warning:
            result += "warning: ";
            break;
        case lispb::DiagnosticSeverity::error:
            result += "error: ";
            break;
    }
    result += message(id);
    return result;
}
void EditorDiagnostics::clear() {
    severities_.clear();
    messages_.clear();
    paths_.clear();
    lines_.clear();
    columns_.clear();
    documents_.clear();
    operations_.clear();
    instructions_.clear();
}

} // namespace ioj::layout
