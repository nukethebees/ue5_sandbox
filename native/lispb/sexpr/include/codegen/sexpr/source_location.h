#pragma once

#include <cstdint>
#include <string>
#include <string_view>
namespace lispb::codegen::sexpr {

struct SourceLocation {
    std::uint32_t line{1};
    std::uint32_t column{1};
    std::string_view path;
};

class Sources;
struct SourceSpan;
auto format_location(Sources const& sources, SourceSpan span) -> std::string;

}
