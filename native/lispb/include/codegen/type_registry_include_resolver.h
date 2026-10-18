#pragma once

#include <codegen/sexpr/indices.h>

#include <functional>
#include <string_view>

namespace lispb::codegen {
using TypeRegistryIncludeResolver =
    std::function<sexpr::SourceFileId(sexpr::SourceFileId, std::string_view)>;
}
