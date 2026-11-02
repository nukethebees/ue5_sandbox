#pragma once

namespace lispb::codegen {

struct FunctionDefinitionFlags {
    bool is_constexpr{};
    bool is_inline{};
    bool is_nodiscard{};
    bool define_in_header{};
    bool is_noreturn{};
};

}
