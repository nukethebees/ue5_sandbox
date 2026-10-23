#include "lowering_context.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/indices.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/soa_member_schemas.h>
#include <lispb/indices.h>
#include <lispb/schema/resolved_occurrences.h>
#include <lispb/schema/type_graph.h>
#include <lispb/strings.h>

#include <cassert>
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

auto LoweringContext::for_declaration(DeclarationHandle declaration) const -> LoweringContext {
    auto const types{semantic.types_for_declaration(declaration)};
    assert(!types.empty());
    auto result{*this};
    result.module = semantic.module(types.front());
    return result;
}

auto LoweringContext::nested_declaration(SoaMemberId member) const -> DeclarationHandle {
    auto const occurrence{semantic.resolved(
        module, declarations.soa_members.type[declarations.soa_members.index(member)])};
    auto const& occurrences{semantic.occurrences()};
    return semantic.source_declaration(occurrences.type[occurrences.index(occurrence)]);
}
auto LoweringContext::text(StringId value) const -> std::string {
    return std::string{declarations.strings().view(value)};
}

auto LoweringContext::optional_text(std::optional<StringId> value) const
    -> std::optional<std::string> {
    return value ? std::optional{text(*value)} : std::nullopt;
}

}
