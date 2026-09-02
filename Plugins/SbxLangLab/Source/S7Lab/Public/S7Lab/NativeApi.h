#pragma once

#include <CoreTypes.h>

struct s7_scheme;
struct s7_cell;

namespace S7Lab::native {
using FValue = s7_cell*;

S7LAB_API auto is_list(s7_scheme& scheme, FValue value) -> bool;
S7LAB_API auto list_length(s7_scheme& scheme, FValue value) -> int64;
S7LAB_API auto list_value(s7_scheme& scheme, FValue value, int64 index) -> FValue;
S7LAB_API auto is_symbol(FValue value) -> bool;
S7LAB_API auto symbol_name(FValue value) -> ANSICHAR const*;
S7LAB_API auto is_string(FValue value) -> bool;
S7LAB_API auto string_value(FValue value) -> ANSICHAR const*;
S7LAB_API auto is_real(FValue value) -> bool;
S7LAB_API auto number_to_real(s7_scheme& scheme, FValue value) -> double;
}
