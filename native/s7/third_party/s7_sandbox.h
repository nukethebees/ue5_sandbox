#pragma once

#include "s7.h"

auto s7_symbol_force_set_initial_value(s7_scheme* scheme, s7_pointer symbol, s7_pointer value)
    -> s7_pointer;
