#pragma once

#include <codegen/ast/symbol_binding.h>

#include <cstddef>

namespace lispb::codegen {

struct ResolvedName {
    SymbolBinding symbol;
    // Retain the unresolved suffix of dependent or external member references.
    std::size_t prefix_size{};
};

}
