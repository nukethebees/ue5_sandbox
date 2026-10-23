#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class EnvironmentKind : std::uint8_t {
    global,
    namespace_,
    record,
    function,
    block,
    template_,
    external,
};

}
