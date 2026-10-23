#pragma once

#include <ioj/layout/document_id.hpp>
#include <ioj/layout/instruction_id.hpp>
#include <ioj/layout/operation_id.hpp>

#include <lispb/diagnostic_severity.h>
#include <lispb/indices.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lispb {
class Strings;
}
namespace ioj::layout {

// Own diagnostic text in the session string store; no coordinate references scratch storage.
class EditorDiagnostics {
  public:
    explicit EditorDiagnostics(lispb::Strings& strings);
    auto add(DocumentId document,
             OperationId operation,
             std::optional<InstructionId> instruction,
             lispb::DiagnosticSeverity severity,
             std::string_view message,
             std::string_view path = {},
             std::uint32_t line = 0,
             std::uint32_t column = 0) -> lispb::DiagnosticId;

    auto size() const -> std::size_t;
    auto severity(lispb::DiagnosticId id) const -> lispb::DiagnosticSeverity;
    auto message(lispb::DiagnosticId id) const -> std::string_view;
    auto path(lispb::DiagnosticId id) const -> std::string_view;
    auto line(lispb::DiagnosticId id) const -> std::uint32_t;
    auto column(lispb::DiagnosticId id) const -> std::uint32_t;
    auto document(lispb::DiagnosticId id) const -> DocumentId;
    auto operation(lispb::DiagnosticId id) const -> OperationId;
    auto instruction(lispb::DiagnosticId id) const -> std::optional<InstructionId>;
    auto format(lispb::DiagnosticId id) const -> std::string;
    void clear();
  private:
    lispb::Strings& strings_;
    std::vector<lispb::DiagnosticSeverity> severities_;
    std::vector<lispb::StringId> messages_;
    std::vector<lispb::StringId> paths_;
    std::vector<std::uint32_t> lines_;
    std::vector<std::uint32_t> columns_;
    std::vector<DocumentId> documents_;
    std::vector<OperationId> operations_;
    std::vector<std::optional<InstructionId>> instructions_;
};

} // namespace ioj::layout
