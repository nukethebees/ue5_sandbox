#include <kernel_codegen/binary_operator.h>

#include <array>
#include <cstddef>

namespace kernel_codegen::detail {
auto operator_spelling(BinaryOperator const operation) -> char {
    constexpr std::array spellings{'+', '-', '*', '/'};
    return spellings[static_cast<std::size_t>(operation)];
}
}
