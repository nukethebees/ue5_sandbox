#pragma once
#include <codegen/ast/include_style.h>

#include <string_view>
namespace lispb::codegen {
auto include_is_system(std::string_view path, IncludeStyle style = IncludeStyle::inferred) -> bool;
}
