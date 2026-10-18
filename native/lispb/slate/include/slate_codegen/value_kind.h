#pragma once

#include <cstdint>
namespace slate_codegen::detail {

enum class ValueKind : std::uint8_t {
    none,
    number,
    boolean,
    text,
    localized_text,
    symbol,
    callback,
    method,
    uobject,
    margin
};

}
