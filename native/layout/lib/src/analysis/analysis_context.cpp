#include <ioj/layout/analysis/analysis_context.hpp>

#include <ioj/layout/document.hpp>
#include <ioj/layout/editor_session.hpp>
namespace ioj::layout {
AnalysisContext::AnalysisContext(Document const& document_value, AnalysisStorage& output)
    : document{document_value}
    , types{document_value.types()}
    , storage{output} {}
void AnalysisContext::report(Diagnostic const& diagnostic) const {
    document.session().record_diagnostic(diagnostic.severity == DiagnosticSeverity::error
                                             ? lispb::DiagnosticSeverity::error
                                             : lispb::DiagnosticSeverity::warning,
                                         diagnostic.message);
}
}
