#pragma once

#include <codegen/ast/cpp_types.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/registered_types.h>
#include <codegen/sexpr/sources.h>
#include <codegen/sexpr/syntax.h>
#include <lispb/diagnostics.h>
#include <lispb/strings.h>

namespace lispb {

// Keep stores at stable addresses while a compiler or editable document owns their handles.
struct CompilerStorage {
    CompilerStorage() = default;
    CompilerStorage(CompilerStorage const&) = delete;
    CompilerStorage(CompilerStorage&&) = delete;
    auto operator=(CompilerStorage const&) -> CompilerStorage& = delete;
    auto operator=(CompilerStorage&&) -> CompilerStorage& = delete;

    // Release every external handle before resetting. Store addresses and reusable
    // capacity remain stable; source text is discarded along with parsed rows.
    void reset();

    Strings strings;
    Diagnostics diagnostics{strings};
    codegen::Declarations declarations{strings};
    codegen::sexpr::Sources sources{strings, diagnostics};
    codegen::sexpr::Syntax syntax{strings, sources};
    codegen::CppTypes cpp_types{strings};
    codegen::RegisteredTypes registered_types{cpp_types, declarations};
};

} // namespace lispb
