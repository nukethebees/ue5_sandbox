#pragma once

#include <codegen/sexpr/source_span.h>
#include <lispb/diagnostic_severity.h>
#include <lispb/indices.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace lispb::codegen::sexpr {
class Sources;
}

namespace lispb {

class Diagnostics {
  public:
    explicit Diagnostics(Strings& strings);
    auto add(DiagnosticSeverity severity, codegen::sexpr::SourceSpan span, std::string_view message)
        -> DiagnosticId;
    [[noreturn]] void fail(codegen::sexpr::Sources const& sources,
                           codegen::sexpr::SourceSpan span,
                           std::string_view message);
    auto size() const -> std::size_t;
    auto severity(DiagnosticId id) const -> DiagnosticSeverity;
    auto span(DiagnosticId id) const -> codegen::sexpr::SourceSpan;
    auto message(DiagnosticId id) const -> std::string_view;
    auto format(DiagnosticId id, codegen::sexpr::Sources const& sources) const -> std::string;
    void clear();
  private:
    Strings& strings_;
    std::vector<DiagnosticSeverity> severities_;
    std::vector<codegen::sexpr::SourceSpan> spans_;
    std::vector<StringId> messages_;
};

}
