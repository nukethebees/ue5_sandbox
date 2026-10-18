#pragma once

#include <codegen/ast/function_exception_kind.h>

#include <cstdint>

namespace lispb::codegen {

class CppIR;

class FunctionExceptionId {
  public:
    FunctionExceptionId() = default;
    auto kind() const -> FunctionExceptionKind {
        return static_cast<FunctionExceptionKind>(value_ >> 24);
    }
    auto index() const -> std::uint32_t { return value_ & 0x00ffffffU; }
  private:
    friend class CppIR;
    explicit FunctionExceptionId(std::uint32_t value)
        : value_{value} {}
    std::uint32_t value_{};
};

static_assert(sizeof(FunctionExceptionId) == 4);

}
