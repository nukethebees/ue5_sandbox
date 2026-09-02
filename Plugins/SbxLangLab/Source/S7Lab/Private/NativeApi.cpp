#include <S7Lab/NativeApi.h>

#include "s7.h"

namespace S7Lab::native {
auto is_list(s7_scheme& scheme, FValue const value) -> bool {
    return s7_is_list(&scheme, value);
}

auto list_length(s7_scheme& scheme, FValue const value) -> int64 {
    return s7_list_length(&scheme, value);
}

auto list_value(s7_scheme& scheme, FValue const value, int64 const index) -> FValue {
    return s7_list_ref(&scheme, value, index);
}

auto is_symbol(FValue const value) -> bool {
    return s7_is_symbol(value);
}

auto symbol_name(FValue const value) -> ANSICHAR const* {
    return s7_symbol_name(value);
}

auto is_string(FValue const value) -> bool {
    return s7_is_string(value);
}

auto string_value(FValue const value) -> ANSICHAR const* {
    return s7_string(value);
}

auto is_real(FValue const value) -> bool {
    return s7_is_real(value);
}

auto number_to_real(s7_scheme& scheme, FValue const value) -> double {
    return s7_number_to_real(&scheme, value);
}

}
