#pragma once

#include <codegen/sexpr/reader.h>
#include <codegen/sexpr/syntax.h>
#include <slate_codegen/document.h>

#include <string_view>
#include <vector>

namespace slate_codegen::detail {

void parse(lispb::codegen::sexpr::Syntax& syntax,
           lispb::codegen::sexpr::HandleRange forms,
           Document& document);

}
