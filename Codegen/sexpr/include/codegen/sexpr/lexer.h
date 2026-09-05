#pragma once

#include <codegen/sexpr/syntax.h>

#include <string_view>
#include <vector>

namespace codegen::sexpr {

auto lex(std::string_view path, std::string_view source) -> std::vector<Token>;

}
