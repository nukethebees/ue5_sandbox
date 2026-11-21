#pragma once

#include <codegen/sexpr/handle_range.h>
#include <codegen/sexpr/indices.h>

namespace lispb::codegen::sexpr {
class Syntax;

[[nodiscard]] auto read_forms(Syntax& syntax, SourceFileId file) -> HandleRange;
// Recovered forms are for editor queries and never enter the strict reader cache.
[[nodiscard]] auto read_editor_forms(Syntax& syntax, SourceFileId file) -> HandleRange;

}
