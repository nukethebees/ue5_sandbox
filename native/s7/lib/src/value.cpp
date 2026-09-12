#include <native/s7/value.h>

#include "s7.h"

namespace ml::s7 {
auto is_list(Scheme& scheme, Value const value) -> bool {
    return s7_is_list(&scheme, value);
}
auto list_length(Scheme& scheme, Value const value) -> std::int64_t {
    return static_cast<std::int64_t>(s7_list_length(&scheme, value));
}
auto list_value(Scheme& scheme, Value const value, std::int64_t const index) -> Value {
    return s7_list_ref(&scheme, value, static_cast<s7_int>(index));
}
auto is_symbol(Value const value) -> bool {
    return s7_is_symbol(value);
}
auto symbol_name(Value const value) -> std::string_view {
    return s7_symbol_name(value);
}
auto is_string(Value const value) -> bool {
    return s7_is_string(value);
}
auto string_value(Value const value) -> std::string_view {
    return s7_string(value);
}
auto is_real(Value const value) -> bool {
    return s7_is_real(value);
}
auto number_to_real(Scheme& scheme, Value const value) -> double {
    return s7_number_to_real(&scheme, value);
}
}
