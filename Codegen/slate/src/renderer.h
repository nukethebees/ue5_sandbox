#pragma once

#include "syntax.h"

#include <string>

namespace slate_codegen::detail {

auto render(std::string source_path, Child const& root) -> std::string;

}
