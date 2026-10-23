#include <lispb/diagnostics.h>

#include <codegen/sexpr/source_error.h>
#include <codegen/sexpr/sources.h>
#include <lispb/strings.h>

#include <cstdint>

namespace lispb {

Diagnostics::Diagnostics(Strings& strings)
    : strings_{strings} {}
auto Diagnostics::add(DiagnosticSeverity severity,
                      codegen::sexpr::SourceSpan span,
                      std::string_view message) -> DiagnosticId {
    auto const id{DiagnosticId{static_cast<std::uint32_t>(messages_.size())}};
    auto const text{strings_.intern(message)};
    severities_.push_back(severity);
    spans_.push_back(span);
    messages_.push_back(text);
    return id;
}
void Diagnostics::fail(codegen::sexpr::Sources const& sources,
                       codegen::sexpr::SourceSpan span,
                       std::string_view message) {
    throw codegen::sexpr::SourceError{
        *this, sources, add(DiagnosticSeverity::error, span, message)};
}
auto Diagnostics::size() const -> std::size_t {
    return messages_.size();
}
auto Diagnostics::severity(DiagnosticId id) const -> DiagnosticSeverity {
    return severities_[id.value];
}
auto Diagnostics::span(DiagnosticId id) const -> codegen::sexpr::SourceSpan {
    return spans_[id.value];
}
auto Diagnostics::message(DiagnosticId id) const -> std::string_view {
    return strings_.view(messages_[id.value]);
}
auto Diagnostics::format(DiagnosticId id, codegen::sexpr::Sources const& sources) const
    -> std::string {
    auto const location{span(id)};
    auto result{location.file.value != 0 ? codegen::sexpr::format_location(sources, location) + ": "
                                         : std::string{}};
    switch (severity(id)) {
        case DiagnosticSeverity::note:
            result += "note: ";
            break;
        case DiagnosticSeverity::warning:
            result += "warning: ";
            break;
        case DiagnosticSeverity::error:
            result += "error: ";
            break;
    }
    result += message(id);
    result += sources.expansion_text(location);
    return result;
}
void Diagnostics::clear() {
    severities_.clear();
    spans_.clear();
    messages_.clear();
}

}
