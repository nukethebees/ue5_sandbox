#pragma once

#include <codegen/sexpr/handle_range.h>
#include <codegen/sexpr/indices.h>

namespace lispb::codegen::sexpr {
class Syntax;

[[nodiscard]] auto read_forms(Syntax& syntax, SourceFileId file) -> HandleRange;

}
