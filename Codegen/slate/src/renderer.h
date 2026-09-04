#pragma once

#include "syntax.h"

#include <string>

namespace slate_codegen::detail {

auto render(std::string source_path, WidgetClass const& widget_class) -> std::string;

}
