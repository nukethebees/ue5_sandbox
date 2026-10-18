#include <ioj/layout/editor_session.hpp>

#include "document_io.hpp"

#include <codegen/sexpr/source_error.h>

#include <cassert>
#include <limits>
#include <stdexcept>

namespace ioj::layout {

auto EditorSession::from_source(std::string_view const source, std::string_view const registry)
    -> Document {
    DocumentSource const input{"document.lispb", source};
    return from_sources(std::span{&input, 1}, registry);
}
auto EditorSession::from_sources(std::span<DocumentSource const> const sources,
                                 std::string_view const registry) -> Document {
    auto const id{create_document()};
    begin_operation(id);
    Document document{*this, id};
    auto& record{state(id)};
    try {
        document_io::initialize_inputs(record, sources, registry);
        capture_diagnostics(id);
        if (document.replay(0)) {
            document_io::finish_load(record);
        }
    } catch (lispb::codegen::sexpr::SourceError const&) {
        capture_diagnostics(id);
    } catch (std::invalid_argument const& error) {
        record_error(error.what());
    } catch (std::runtime_error const& error) {
        record_error(error.what());
    }
    return document;
}
auto EditorSession::load(std::filesystem::path const& project_path, std::string_view const target)
    -> Document {
    auto const id{create_document()};
    begin_operation(id);
    Document document{*this, id};
    auto& record{state(id)};
    try {
        document_io::load_inputs(record, project_path, target);
        capture_diagnostics(id);
        if (document.replay(0)) {
            document_io::finish_load(record);
        }
    } catch (lispb::codegen::sexpr::SourceError const&) {
        capture_diagnostics(id);
    } catch (std::invalid_argument const& error) {
        record_error(error.what());
    } catch (std::runtime_error const& error) {
        record_error(error.what());
    }
    return document;
}
auto EditorSession::command_strings() -> lispb::Strings& {
    return command_strings_;
}
auto EditorSession::command_strings() const -> lispb::Strings const& {
    return command_strings_;
}
auto EditorSession::storage(DocumentId const document) -> lispb::CompilerStorage& {
    auto const found{workspaces_.find(document)};
    assert(found != workspaces_.end());
    return found->second;
}
auto EditorSession::state(DocumentId const document) -> DocumentState& {
    auto const found{states_.find(document)};
    assert(found != states_.end());
    return found->second;
}
auto EditorSession::state(DocumentId const document) const -> DocumentState const& {
    auto const found{states_.find(document)};
    assert(found != states_.end());
    return found->second;
}
auto EditorSession::create_document() -> DocumentId {
    assert(next_document_id_.value < std::numeric_limits<std::uint32_t>::max());
    auto const id{next_document_id_};
    ++next_document_id_.value;
    workspaces_.try_emplace(id);
    states_.try_emplace(id, *this, id);
    return id;
}
void EditorSession::release_document(DocumentId const document) {
    assert(states_.contains(document));
    states_.erase(document);
    workspaces_.erase(document);
}
void EditorSession::reset_storage(DocumentId const document) {
    storage(document).reset();
}
auto EditorSession::diagnostics() const -> EditorDiagnostics const& {
    return diagnostics_;
}
auto EditorSession::format_diagnostic(lispb::DiagnosticId const diagnostic) const -> std::string {
    return diagnostics_.format(diagnostic);
}
auto EditorSession::last_diagnostic_begin() const -> std::size_t {
    return diagnostic_begin_;
}
void EditorSession::begin_operation(DocumentId const document) {
    assert(current_operation_.value < std::numeric_limits<std::uint32_t>::max());
    diagnostic_begin_ = diagnostics_.size();
    current_document_ = document;
    ++current_operation_.value;
    current_instruction_.reset();
}
void EditorSession::set_instruction(std::optional<InstructionId> const instruction) {
    current_instruction_ = instruction;
}
void EditorSession::record_error(std::string_view const message) {
    record_diagnostic(lispb::DiagnosticSeverity::error, message);
}
auto EditorSession::record_diagnostic(lispb::DiagnosticSeverity const severity,
                                      std::string_view const message,
                                      std::string_view const path,
                                      std::uint32_t const line,
                                      std::uint32_t const column) -> lispb::DiagnosticId {
    return diagnostics_.add(current_document_,
                            current_operation_,
                            current_instruction_,
                            severity,
                            message,
                            path,
                            line,
                            column);
}
void EditorSession::capture_diagnostics(DocumentId const document) {
    auto& scratch{storage(document)};
    auto const count{scratch.diagnostics.size()};
    for (std::uint32_t index{}; index < count; ++index) {
        auto const id{lispb::DiagnosticId{index}};
        auto const span{scratch.diagnostics.span(id)};
        auto const location{scratch.sources.location(span)};
        auto message{std::string{scratch.diagnostics.message(id)}};
        message += scratch.sources.expansion_text(span);
        diagnostics_.add(document,
                         current_operation_,
                         current_instruction_,
                         scratch.diagnostics.severity(id),
                         message,
                         location.path,
                         span.file.value == 0 ? 0 : location.line,
                         span.file.value == 0 ? 0 : location.column);
    }
    scratch.diagnostics.clear();
}

} // namespace ioj::layout
