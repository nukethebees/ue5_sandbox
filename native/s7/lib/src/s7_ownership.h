#pragma once

#include "s7.h"

namespace ml::s7::detail {
// Use only around host operations that return normally or throw C++ exceptions.
// Scheme evaluation must contain its non-local exits inside an s7 catch boundary.
class GcProtection final {
  public:
    GcProtection(s7_scheme* scheme, s7_pointer value)
        : scheme_{scheme}
        , value_{value}
        , location_{s7_gc_protect(scheme, value)} {}
    ~GcProtection() { s7_gc_unprotect_at(scheme_, location_); }

    GcProtection(GcProtection const&) = delete;
    auto operator=(GcProtection const&) -> GcProtection& = delete;

    [[nodiscard]] auto get() const -> s7_pointer { return value_; }
  private:
    s7_scheme* scheme_;
    s7_pointer value_;
    s7_int location_;
};
}
