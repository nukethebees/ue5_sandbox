#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class FunctionReceiver : std::uint8_t {
    unqualified,
    const_,
    lvalue,
    const_lvalue,
    rvalue,
    const_rvalue,
    static_
};

}
