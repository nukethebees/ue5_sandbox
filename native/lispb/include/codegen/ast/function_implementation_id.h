#pragma once

#include <codegen/ast/function_implementation_kind.h>

#include <cstdint>

namespace lispb::codegen {

class CppIR;

class FunctionImplementationId {
    inline static constexpr std::uint32_t index_mask{0x00ffffffu};
    inline static constexpr std::uint32_t kind_shift{24};
  public:
    FunctionImplementationId() = default;
    auto kind() const -> FunctionImplementationKind {
        return static_cast<FunctionImplementationKind>(value_ >> kind_shift);
    }
    auto index() const -> std::uint32_t { return value_ & index_mask; }
  private:
    friend class CppIR;
    explicit FunctionImplementationId(std::uint32_t value)
        : value_{value} {}
    std::uint32_t value_{};
};

static_assert(sizeof(FunctionImplementationId) == 4);

}
