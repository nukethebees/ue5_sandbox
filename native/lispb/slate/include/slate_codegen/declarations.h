#pragma once

#include <codegen/sexpr/source_span.h>
#include <lispb/indices.h>
#include <slate_codegen/declaration_kind.h>
#include <slate_codegen/range.h>

#include <cstddef>
#include <vector>
namespace slate_codegen::detail {

struct Declarations {
    std::vector<lispb::StringId> names;
    std::vector<Range> functions;
    std::vector<DeclarationKind> kinds;
    std::vector<lispb::codegen::sexpr::SourceSpan> spans;
    [[nodiscard]] auto size() const -> std::size_t { return names.size(); }
    [[nodiscard]] auto empty() const -> bool { return names.empty(); }
};

}
