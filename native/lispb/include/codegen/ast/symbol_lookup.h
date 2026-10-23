#pragma once
#include <codegen/ast/environment_symbol.h>
#include <codegen/ast/lookup_status.h>

namespace lispb::codegen {
struct SymbolLookup {
    LookupStatus status{LookupStatus::missing};
    EnvironmentSymbol symbol;
};
}
