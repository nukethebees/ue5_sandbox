#pragma once

#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/token_range.h>

namespace lispb::codegen::sexpr {
class Syntax;

[[nodiscard]] auto lex(Syntax& syntax, SourceFileId file) -> TokenRange;

}
