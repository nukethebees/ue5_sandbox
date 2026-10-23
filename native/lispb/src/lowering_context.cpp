#include "lowering_context.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/indices.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/indices.h>
#include <lispb/indices.h>
#include <lispb/schema/resolved_occurrences.h>
#include <lispb/schema/type_graph.h>
#include <lispb/strings.h>

#include <optional>
#include <string>

namespace lispb::codegen::detail {

auto LoweringContext::path_text(LoweringPathId path, std::string_view separator) const
    -> std::string {
    std::string result;
    for (auto const name : ir.lowering.paths.items(path)) {
        if (!result.empty()) {
            result += separator;
        }
        result += declarations.strings().view(name);
    }
    return result;
}

auto LoweringContext::resolve(TypeRefId reference) const -> CppTypeId {
    auto const occurrence{semantic.resolved(module, reference)};
    auto const& occurrences{semantic.occurrences()};
    return occurrences.cpp_type[occurrences.index(occurrence)];
}
auto LoweringContext::text(StringId value) const -> std::string {
    return std::string{declarations.strings().view(value)};
}

auto LoweringContext::optional_text(std::optional<StringId> value) const
    -> std::optional<std::string> {
    return value ? std::optional{text(*value)} : std::nullopt;
}

}
