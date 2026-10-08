#pragma once

#include <s7.h>

#include <cstdint>

namespace ioj::s7::detail {
// Use only around host operations that return normally or throw C++ exceptions.
// Scheme evaluation must contain its non-local exits inside an s7 catch boundary.
class GcProtection {
  public:
    GcProtection(s7_scheme* scheme, s7_pointer value);
    ~GcProtection();

    GcProtection(GcProtection const&) = delete;
    auto operator=(GcProtection const&) -> GcProtection& = delete;

    [[nodiscard]] auto get() const -> s7_pointer { return value_; }
  private:
    s7_scheme* scheme_;
    s7_pointer value_;
    std::int64_t location_;
};
}
