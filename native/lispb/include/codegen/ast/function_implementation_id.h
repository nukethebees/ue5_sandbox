#pragma once

#include <codegen/ast/function_implementation_kind.h>

#include <cstdint>

namespace lispb::codegen {

class CppIR;

class FunctionImplementationId {
  public:
    FunctionImplementationId() = default;
    auto kind() const -> FunctionImplementationKind {
        return static_cast<FunctionImplementationKind>(value_ >> 24);
    }
    auto index() const -> std::uint32_t { return value_ & 0x00ffffffU; }
  private:
    friend class CppIR;
    explicit FunctionImplementationId(std::uint32_t value)
        : value_{value} {}
    std::uint32_t value_{};
};

static_assert(sizeof(FunctionImplementationId) == 4);

}
