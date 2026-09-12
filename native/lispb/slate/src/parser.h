#pragma once

#include "syntax.h"

#include <string_view>
#include <vector>

namespace slate_codegen::detail {

auto parse(std::string_view path, std::vector<Token> tokens) -> Document;

}
