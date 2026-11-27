#pragma once

#include <ioj/input/key_code.h>
#include <ioj/s7/writer.h>

namespace ioj::s7 {
template <>
struct TypeWriter<FlightMapping> {
    static auto write(SExprIR& ir, FlightMapping value, FlightMapping const*) -> Expr {
        return ir.string(std::string{to_serialized_string(value)});
    }
};
template <>
struct TypeWriter<KeyCode> {
    static auto write(SExprIR& ir, KeyCode value, KeyCode const*) -> Expr {
        return ir.string(serialized_key(value));
    }
};
}
