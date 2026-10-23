#pragma once

#include <codegen/ast/function_exception_kind.h>

#include <cstdint>

namespace lispb::codegen {

class CppIR;

class FunctionExceptionId {
    inline static constexpr std::uint32_t index_mask{0x00ffffffu};
    inline static constexpr std::uint32_t kind_shift{24};
  public:
    FunctionExceptionId() = default;
    auto kind() const -> FunctionExceptionKind {
        return static_cast<FunctionExceptionKind>(value_ >> kind_shift);
    }
    auto index() const -> std::uint32_t { return value_ & index_mask; }
  private:
    friend class CppIR;
    explicit FunctionExceptionId(std::uint32_t value)
        : value_{value} {}
    std::uint32_t value_{};
};

static_assert(sizeof(FunctionExceptionId) == 4);

}
