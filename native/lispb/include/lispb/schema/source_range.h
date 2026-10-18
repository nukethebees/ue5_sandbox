#pragma once

#include <codegen/schema/declaration_handle.h>
#include <codegen/sexpr/indices.h>

namespace lispb::schema {
struct SourceRange {
    codegen::sexpr::FormId form;
    codegen::DeclarationHandle original;

    auto operator==(SourceRange const&) const -> bool = default;
};
}
