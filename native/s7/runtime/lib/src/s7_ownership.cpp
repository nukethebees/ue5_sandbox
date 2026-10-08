#include "s7_ownership.h"

#include "s7.h"

namespace ioj::s7::detail {
GcProtection::GcProtection(s7_scheme* const scheme, s7_pointer const value)
    : scheme_{scheme}
    , value_{value}
    , location_{s7_gc_protect(scheme, value)} {}
GcProtection::~GcProtection() {
    s7_gc_unprotect_at(scheme_, location_);
}
}
