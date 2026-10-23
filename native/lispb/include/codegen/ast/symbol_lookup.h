#pragma once
#include <codegen/ast/lookup_status.h>
#include <codegen/ast/symbol_binding.h>

namespace lispb::codegen {
struct SymbolLookup {
    LookupStatus status{LookupStatus::missing};
    SymbolBinding symbol;
};
}
