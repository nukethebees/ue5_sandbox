#pragma once

#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/token_range.h>

namespace lispb::codegen::sexpr {
class Syntax;
enum class LexMode { strict, editor };

[[nodiscard]] auto lex(Syntax& syntax, SourceFileId file, LexMode mode = LexMode::strict)
    -> TokenRange;

}
