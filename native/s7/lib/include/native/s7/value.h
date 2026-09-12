#pragma once

#include <cstdint>
#include <string_view>

struct s7_scheme;
struct s7_cell;

namespace ml::s7 {
using Scheme = s7_scheme;
using Value = s7_cell*;

[[nodiscard]] auto is_list(Scheme& scheme, Value value) -> bool;
[[nodiscard]] auto list_length(Scheme& scheme, Value value) -> std::int64_t;
[[nodiscard]] auto list_value(Scheme& scheme, Value value, std::int64_t index) -> Value;
[[nodiscard]] auto is_symbol(Value value) -> bool;
[[nodiscard]] auto symbol_name(Value value) -> std::string_view;
[[nodiscard]] auto is_string(Value value) -> bool;
[[nodiscard]] auto string_value(Value value) -> std::string_view;
[[nodiscard]] auto is_real(Value value) -> bool;
[[nodiscard]] auto number_to_real(Scheme& scheme, Value value) -> double;
}
