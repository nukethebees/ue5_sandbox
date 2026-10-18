#pragma once

#include <ioj/layout/document.hpp>
#include <ioj/layout/document_id.hpp>
#include <ioj/layout/document_source.hpp>
#include <ioj/layout/document_state.hpp>
#include <ioj/layout/editor_diagnostics.hpp>
#include <ioj/layout/instruction_id.hpp>
#include <ioj/layout/operation_id.hpp>

#include <lispb/compiler_storage.h>
#include <lispb/strings.h>

#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace ioj::layout {

// Own durable input/history strings and diagnostics separately from per-document workspaces.
// The session must outlive its documents. Editing one document does not reset another's stores.
class EditorSession {
  public:
    EditorSession() = default;
    EditorSession(EditorSession const&) = delete;
    EditorSession(EditorSession&&) = delete;
    auto operator=(EditorSession const&) -> EditorSession& = delete;
    auto operator=(EditorSession&&) -> EditorSession& = delete;

    auto from_source(std::string_view source, std::string_view registry = {}) -> Document;
    auto from_sources(std::span<DocumentSource const> sources, std::string_view registry = {})
        -> Document;
    auto load(std::filesystem::path const& project_path, std::string_view target) -> Document;

    auto command_strings() -> lispb::Strings&;
    auto command_strings() const -> lispb::Strings const&;
    auto storage(DocumentId document) -> lispb::CompilerStorage&;
    auto state(DocumentId document) -> DocumentState&;
    auto state(DocumentId document) const -> DocumentState const&;
    auto create_document() -> DocumentId;
    void release_document(DocumentId document);
    // Clear document-local metadata that borrows scratch handles before calling this.
    void reset_storage(DocumentId document);

    auto diagnostics() const -> EditorDiagnostics const&;
    auto format_diagnostic(lispb::DiagnosticId diagnostic) const -> std::string;
    auto last_diagnostic_begin() const -> std::size_t;
    void begin_operation(DocumentId document = {});
    void set_instruction(std::optional<InstructionId> instruction);
    void record_error(std::string_view message);
    auto record_diagnostic(lispb::DiagnosticSeverity severity,
                            std::string_view message,
                            std::string_view path = {},
                            std::uint32_t line = 0,
                            std::uint32_t column = 0) -> lispb::DiagnosticId;
    // Publish parser rows once before scratch reset, then empty the scratch diagnostic table.
    void capture_diagnostics(DocumentId document);
  private:
    lispb::Strings command_strings_;
    EditorDiagnostics diagnostics_{command_strings_};
    std::map<DocumentId, lispb::CompilerStorage> workspaces_;
    std::map<DocumentId, DocumentState> states_;
    DocumentId next_document_id_{1};
    DocumentId current_document_{};
    OperationId current_operation_{};
    std::optional<InstructionId> current_instruction_;
    std::size_t diagnostic_begin_{};
};

} // namespace ioj::layout
