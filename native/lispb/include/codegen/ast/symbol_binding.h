#pragma once
#include <codegen/ast/cpp_declaration_id.h>
#include <codegen/ast/environment_id.h>

namespace lispb::codegen {
struct SymbolBinding {
    CppDeclarationId declaration;
    // Distinguish declarations when an IR node is reused in another environment.
    EnvironmentId declaring_scope;
    EnvironmentId introduced_scope;
    bool ambiguous{};

    auto same_declaration(SymbolBinding const& other) const -> bool {
        return declaration == other.declaration && declaring_scope == other.declaring_scope;
    }
};
}
