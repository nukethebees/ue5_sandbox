#pragma once

#include "syntax.h"

#include <codegen/sexpr/reader.h>

#include <string_view>
#include <vector>

namespace kernel_codegen::detail {

auto parse(std::string_view path, std::vector<codegen::sexpr::Form> forms) -> Document;

}
