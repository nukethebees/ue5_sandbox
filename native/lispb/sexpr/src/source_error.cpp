#include <codegen/sexpr/source_error.h>

#include <lispb/diagnostics.h>

namespace lispb::codegen::sexpr {

SourceError::SourceError(Diagnostics const& diagnostics,
                         Sources const& sources,
                         DiagnosticId diagnostic)
    : std::runtime_error{diagnostics.format(diagnostic, sources)}
    , diagnostic_{diagnostic} {}

}
