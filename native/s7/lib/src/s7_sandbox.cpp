// This translation unit exposes the one internal operation needed to revoke S7's #_name bindings.
#include "../../ThirdParty/s7/s7.cpp"

s7_pointer
    s7_symbol_force_set_initial_value(s7_scheme* scheme, s7_pointer symbol, s7_pointer value) {
    set_initial_value(symbol, value);
    if (in_heap(value)) {
        add_semipermanent_object(scheme, value);
    }
    return value;
}
