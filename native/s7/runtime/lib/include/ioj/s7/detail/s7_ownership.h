#pragma once

#include <ioj/s7/value.h>

#include <cstdint>

namespace ioj::s7::detail {
// Use only around host operations that return normally or throw C++ exceptions.
// Scheme evaluation must contain its non-local exits inside an s7 catch boundary.
class GcProtection {
  public:
    GcProtection(Scheme* scheme, Value value);
    ~GcProtection();

    GcProtection(GcProtection const&) = delete;
    auto operator=(GcProtection const&) -> GcProtection& = delete;

    [[nodiscard]] auto get() const -> Value { return value_; }
  private:
    Scheme* scheme_;
    Value value_;
    std::int64_t location_;
};
}
