#include <codegen/ast/include.h>

#include <codegen/ast/include_style.h>
namespace lispb::codegen {
auto include_is_system(std::string_view path, IncludeStyle style) -> bool {
    if (style != IncludeStyle::inferred) {
        return style == IncludeStyle::system;
    }
    auto const slash{path.find_last_of('/')};
    return !path.substr(slash == std::string_view::npos ? 0 : slash + 1).contains('.');
}
}
