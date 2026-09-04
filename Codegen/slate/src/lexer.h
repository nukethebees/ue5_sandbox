#pragma once

#include "syntax.h"

#include <string_view>
#include <vector>

namespace slate_codegen::detail {

auto lex(std::string_view path, std::string_view source) -> std::vector<Token>;

}
