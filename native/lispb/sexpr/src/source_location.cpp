#include <codegen/sexpr/source_location.h>

#include <codegen/sexpr/source_error.h>
#include <codegen/sexpr/source_span.h>
#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>

#include <cassert>
#include <string>

namespace lispb::codegen::sexpr {

auto format_location(Sources const& sources, SourceSpan const span) -> std::string {
    auto const location{sources.location(span)};
    return std::string{location.path} + ":" + std::to_string(location.line) + ":" +
           std::to_string(location.column);
}

}
