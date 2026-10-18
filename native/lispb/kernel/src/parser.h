#pragma once

#include <codegen/sexpr/handle_range.h>
#include <codegen/sexpr/indices.h>
#include <codegen/sexpr/syntax.h>
#include <kernel_codegen/document.h>

#include <string_view>
#include <vector>

namespace kernel_codegen::detail {

void parse(lispb::codegen::sexpr::Syntax& syntax,
           Document& document,
           lispb::codegen::sexpr::SourceFileId file,
           lispb::codegen::sexpr::HandleRange forms);

}
