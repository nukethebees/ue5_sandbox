#include <codegen/sexpr/source_error.h>

#include <codegen/sexpr/source_location.h>
#include <codegen/sexpr/source_span.h>
#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>

#include <cassert>
#include <stdexcept>
#include <string>

namespace lispb::codegen::sexpr {

namespace {
auto format_error(Sources const& sources, SourceSpan const span, std::string const& message)
    -> std::string {
    return format_location(sources, span) + ": error: " + message + sources.expansion_text(span);
}

}
SourceError::SourceError(Sources const& sources, SourceSpan const span, std::string const& message)
    : std::runtime_error{format_error(sources, span, message)}
    , path_{sources.path(span.file)}
    , span_{span}
    , line_{sources.location(span).line}
    , column_{sources.location(span).column}
    , message_{message} {}

auto SourceError::location() const -> SourceLocation {
    return {line_, column_, path_};
}

}
