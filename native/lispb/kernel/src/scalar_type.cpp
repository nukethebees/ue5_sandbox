#include <kernel_codegen/scalar_type.h>

#include <array>
#include <cstddef>
#include <string>

namespace kernel_codegen::detail {
auto type_spelling(ScalarType const type) -> std::string {
    constexpr std::array spellings{"int32", "uint32", "float", "double"};
    return spellings[static_cast<std::size_t>(type)];
}
}
