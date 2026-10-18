#pragma once

#include <string>
#include <string_view>

namespace lispb::codegen {
void append_escaped_cpp_string(std::string& result, std::string_view value);
auto escape_cpp_string(std::string_view value) -> std::string;
}
